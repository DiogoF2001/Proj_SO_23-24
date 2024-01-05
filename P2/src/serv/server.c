#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "constants.h"
#include "operations.h"
#include "pipe_parser.h"

//Global Variables
pthread_mutex_t buffer_lock;
pthread_cond_t condit_w;
pthread_cond_t condit_r;
unsigned int writing;
unsigned int reading;
unsigned int count_req;
unsigned int signal_usr1;
unsigned int running;
t_args **buffer;

int main(int argc, char *argv[])
{
	int incoming = -1;
	ssize_t ret = -1;
	char *in_path = NULL;
	char *request = NULL;
	t_args *new_client = NULL;
	pthread_t *tid;
	int i;
	unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

	running = 1;
	signal_usr1 = 0;
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		exit(EXIT_FAILURE);
	}
	if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
		exit(EXIT_FAILURE);
	}

	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	in_path = argv[1];
	if (in_path == NULL)
	{
		fprintf(stderr, "Error getting the name of the pipe!");
		exit(EXIT_FAILURE);
	}

	// Remove pipe if it does not exist
	if (unlink(in_path) != 0 && errno != ENOENT)
	{
		fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", in_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Create pipe
	if (mkfifo(in_path, 0640) != 0)
	{
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

	if (ems_init(state_access_delay_ms)) {
		fprintf(stderr, "Failed to initialize EMS\n");
		exit(1);
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
	// While loop in case of a signal interruption
	while(1){
		incoming = open(in_path, O_RDONLY);
		if (incoming == -1)
		{
			if(errno == EINTR)
				continue;
			fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		else
			break;
	}

	int *ids = malloc(sizeof(int)*MAX_SESSION);
	for(i=0;i<MAX_SESSION;i++){
		ids[i] = i;
	}

	tid = malloc(sizeof(pthread_t) * MAX_SESSION);

	for(i = 0; i < MAX_SESSION; i++){
		pthread_create(&(tid[i]), 0, thread_func, (void*) &(ids[i]));
	}

	while (running){
		/*Flag used for SIGUSR1 and when raised inverts itself*/
		if(signal_usr1){
			ems_list_and_show_events(STDOUT_FILENO);
			raise(SIGUSR1);
		}
		ret = read(incoming, request, sizeof(char) * (1 + 40 + 40));
		switch (ret){
			case -1:
				if(errno == EINTR)
					continue;
				fprintf(stderr, "[Err]: invalid incoming message format\n");
				exit(EXIT_FAILURE);
				break;
			case 0:
				close(incoming);
				while(1){
					incoming = open(in_path, O_RDONLY);
					if (incoming == -1)
					{
						if(errno == EINTR)
							continue;
						fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
						exit(EXIT_FAILURE);
					}
					else
						break;
				}
				continue;
			case 81:
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
				break;
		}
	}

	for(i = 0; i < MAX_SESSION; i++){
		pthread_join(tid[i], NULL);
	}

	free(tid);
	free(ids);
	pthread_mutex_destroy(&buffer_lock);
	pthread_cond_destroy(&condit_w);
	pthread_cond_destroy(&condit_r);
	free(buffer);
	free(request);
	close(incoming);
	unlink(in_path);
	return 0;
}

void *thread_func(void *args)
{
	int session_id = * ((int*) args);
	unsigned int event_id, quit = 0;
	int func_ret = 0, in, out;
	t_args *t;
	size_t num_rows, num_cols, num_coords;
	size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	while (running){
		pthread_mutex_lock(&buffer_lock);
		while (count_req == 0){
			pthread_cond_wait(&condit_r, &buffer_lock);
		}
		if(!running)
			break;;
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

		write(out, &session_id, sizeof(int));

		while(1){
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
					num_coords = pipe_parse_reserve(in, &event_id, xs, ys);
					if(num_coords == 0){
						fprintf(stderr, "Invalid usage of RESERVE operation\n");
					}
					func_ret = ems_reserve(event_id, num_coords, xs, ys);
					write(out, &func_ret, sizeof(int));
					if(func_ret){
						fprintf(stderr, "Failed to reserve seats\n");
					}
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

void signal_handler(int s){
	if(s == SIGUSR1){
		if(signal_usr1)
			signal_usr1 = 0;
		else
			signal_usr1 = 1;
		signal(SIGUSR1, signal_handler);
	}
	else{
		if(s == SIGINT){
			running = 0;
			signal(SIGINT, signal_handler);

		}
	}
}