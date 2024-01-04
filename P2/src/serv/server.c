#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "constants.h"
#include "operations.h"
#include "pipe_parser.h"

void* thread_func(void*);

//Global Variables
pthread_mutex_t buffer_lock;
pthread_cond_t condit_w;
pthread_cond_t condit_r;
unsigned int writing;
unsigned int reading;
unsigned int count_req;
t_args **buffer;

int main(int argc, char *argv[])
{
	int incoming = -1;
	ssize_t ret = -1;
	char *in_path = NULL;
	char *request = NULL;
	t_args *new_client = NULL;
	pthread_t tid;
	int i;

	if(argc < 2 || argc > 3){
		fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	in_path = argv[1];
	if(in_path == NULL){
		fprintf(stderr, "Error getting the name of the pipe!");
		exit(EXIT_FAILURE);
	}

	// Remove pipe if it does not exist
	if (unlink(in_path) != 0 && errno != ENOENT) {
		fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", in_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Create pipe
	if (mkfifo(in_path, 0640) != 0) {
		fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	request = malloc(sizeof(char) * (1 + 40 + 40));
	if(request == NULL){
		fprintf(stderr, "[Err]: allocation of memory for incoming request failed\n");
		exit(EXIT_FAILURE);
	}

	buffer = malloc(sizeof(t_args*) * PROD_CONS_SIZE);
	if(buffer == NULL){
		fprintf(stderr, "[Err]: allocation of memory for prod/cons buffer failed\n");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < PROD_CONS_SIZE; i++){
		buffer[i] = malloc(sizeof(t_args));
		if(buffer[i] == NULL){
			fprintf(stderr, "[Err]: allocation of memory for prod/cons buffer failed\n");
			exit(EXIT_FAILURE);
		}
	}

	pthread_mutex_init(&buffer_lock, NULL);
	pthread_cond_init(&condit_w, NULL);
	pthread_cond_init(&condit_r, NULL);

	reading = 0;
	writing = 0;
	count_req = 0;

	// Open pipe for reading
	// This waits for someone to open it for writing
	incoming = open(in_path, O_RDONLY);
	if (incoming == -1) {
		fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < MAX_SESSION; i++){
		pthread_create(&tid, 0, thread_func, (void*) new_client);
	}

	while (1){
		ret = read(incoming, request, sizeof(char) * (1 + 40 + 40));
		switch (ret){
			case -1:
				fprintf(stderr, "[Err]: invalid incoming message format\n");
				exit(EXIT_FAILURE);
				break;
			case 0:
				continue;
			case 40:
				if(request[0] != '1'){
					fprintf(stderr, "Wrong OP_CODE for this pipe\n");
					continue;
				}
				pthread_mutex_lock(&buffer_lock);
				while (count_req == PROD_CONS_SIZE){
					pthread_cond_wait(&condit_w, &buffer_lock);
				}
				new_client = malloc(sizeof(t_args));
				strcpy(new_client->req, request+1);
				strcpy(new_client->resp, request+1+40);
				buffer[writing] = new_client;
				count_req++;
				pthread_cond_signal(&condit_r);
				writing = (writing + 1) % PROD_CONS_SIZE;
				pthread_mutex_unlock(&buffer_lock);
				break;
			default:
				// FIXME
				break;
		}
	}
	pthread_mutex_destroy(&buffer_lock);
	pthread_cond_destroy(&condit_w);
	pthread_cond_destroy(&condit_r);
	free(buffer);
	free(request);
	close(incoming);
	unlink(in_path);
	return 0;
}

void* thread_func(void *args){
	unsigned int event_id, quit = 0;
	int func_ret = 0, in, out;
	char OP_CODE;
	t_args *t = (t_args*) args;
	ssize_t ret = -1;
	size_t num_rows, num_cols, num_coords;
	size_t *xs = NULL, *ys = NULL;

	while (1/*should be a signal*/){
		pthread_mutex_lock(&buffer_lock);
		while (count_req == 0){
			pthread_cond_wait(&condit_r, &buffer_lock);
		}
		t = buffer[reading];
		in = open(t->req, O_RDONLY);
		out = open(t->resp, O_WRONLY);
		if(in == -1 || out == -1){
			fprintf(stderr, "[ERR]: Opening files:\n- in: %s\n- out: %s\n", t->req, t->resp);
			exit(EXIT_FAILURE);
		}
		free(buffer[reading]);
		reading = (reading + 1) % PROD_CONS_SIZE;
		count_req--;
		pthread_cond_signal(&condit_w);
		pthread_mutex_unlock(&buffer_lock);

		while(1){
			ret = read(in, &OP_CODE, 1);
			if(ret == 0 ){
				break;
			}
			if(ret == -1){
				fprintf(stderr, "[ERR]: read from client pipe failed\n");
				exit(1);
			}
			switch (pipe_get_next(in)){
				case OP_CREATE:
					if(pipe_parse_create(in, &event_id, &num_rows, &num_cols) != 0){
						fprintf(stderr, "Invalid usage of CREATE operation\n");
					}
					func_ret = ems_create(event_id, num_rows, num_cols);
					write(out, &func_ret, sizeof(int));
					if(func_ret){
						fprintf(stderr, "Failed to create event\n");
					}
					break;
				case OP_RESERVE:
					num_coords = pipe_parse_reserve(in, &event_id, &xs, &ys);
					if(num_coords == 0){
						fprintf(stderr, "Invalid usage of RESERVE operation\n");
					}
					func_ret = ems_reserve(event_id, num_coords, xs, ys);
					write(out, &func_ret, sizeof(int));
					if(func_ret){
						fprintf(stderr, "Failed to reserve seats\n");
					}
					free(xs);
					free(ys);
					break;
				case OP_SHOW:
					if(pipe_parse_show(in, &event_id)){
						fprintf(stderr, "Invalid usage of SHOW operation\n");
					}
					func_ret = ems_show(event_id, out);
					if(func_ret){
						fprintf(stderr, "Failed to show event\n");
					}
					break;
				case OP_LIST_EVENTS:
					func_ret = ems_list_events(out);
					if(func_ret){
						fprintf(stderr, "Failed to list events\n");
					}
					break;
				case OP_QUIT:
					quit = 1;
					break;
				case OP_INVALID:
					break;
			}
			if(quit)
				break;
		}
		close(in);
		close(out);
	}
	pthread_exit(0);
}