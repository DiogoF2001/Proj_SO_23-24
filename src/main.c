#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

unsigned int barrier = 0;
long unsigned int barrier_count;
unsigned int active = 0;
unsigned int *wait_ts = NULL;

int main(int argc, char *argv[]) {
	unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
	DIR *dir;
	struct dirent *dp;
	int ret = 1;
	char *in_path, *out_path;
	size_t name_size;
	mode_t write_perms;
	pid_t pid, pid_temp;
	unsigned long int max_proc, max_threads, num_files = 0, i;
	file_s **fds = NULL;
	pthread_t *tid = NULL;
	t_args **args = NULL;
	sem_t sem;

	if(argc < 4){
		fprintf(stderr, "Not enough arguments!!!\n"
		"Correct invocation examples:\n"
		"./ems DIR_PATH MAX_PROC MAX_THREADS\n"
		"./ems DIR_PATH MAX_PROC MAX_THREADS DELAY\n");
		exit(1);
	}

	max_proc = strtoul(argv[2],NULL,10);
	max_threads = strtoul(argv[3],NULL,10);


	if(max_proc > UINT_MAX || max_threads > UINT_MAX){
		fprintf(stderr, "Invalid max value for processes or threads\n");
		exit(1);
	}

	if (argc > 4) {
		char *endptr;
		unsigned long int delay = strtoul(argv[4], &endptr, 10);

		if (*endptr != '\0' || delay > UINT_MAX) {
			fprintf(stderr, "Invalid delay value or value too large\n");
			exit(1);
		}

		state_access_delay_ms = (unsigned int) delay;
	}

	dir = opendir(argv[1]);
	if(dir == NULL){
		fprintf(stderr, "opendir failed on '%s'", argv[1]);
		exit(1);
	}

	write_perms = 00400 | 00200 | 00040 | 00020 | 00004;

	for (;;){
		dp = readdir(dir);
		if(dp == NULL)
			break;
		if(strchr(dp->d_name, '.') == NULL || strcmp(strchr(dp->d_name, '.'), IN_EXT) != 0)
			continue;
		if(num_files >= max_proc){
			wait(&ret);
			if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) > 0)){
				fprintf(stderr, "Child process returned error\n");
				exit(1);
			}
		}
		else
			(num_files)++;
		pid = fork();
		if(pid == 0){
			//free(dir);
			if (ems_init(state_access_delay_ms)) {
				fprintf(stderr, "Failed to initialize EMS\n");
				exit(1);
			}
			in_path = malloc(sizeof(char) * (strlen(argv[1]) + 1 + strlen(dp->d_name) + 1));
			if(in_path == NULL){
				fprintf(stderr, "malloc failed for in_path\n");
				exit(1);
			}
			strcpy(in_path, argv[1]);
			strcat(in_path, "/");
			strcat(in_path, dp->d_name);
			
			name_size = strlen(dp->d_name) - strlen(strchr(dp->d_name, '.'));
			out_path = malloc(sizeof(char) * (strlen(argv[1]) + 1 + name_size + strlen(OUT_EXT) + 1));
			if(out_path == NULL){
				fprintf(stderr, "malloc failed for out_path\n");
				exit(1);
			}
			strcpy(out_path, argv[1]);
			strcat(out_path, "/");
			strncat(out_path, dp->d_name,name_size);
			strcat(out_path, OUT_EXT);

			fds = malloc(sizeof(file_s*)*2);
			if(fds == NULL){
				fprintf(stderr, "malloc failed for fds\n");
				exit(1);
			}
			fds[0] = malloc(sizeof(file_s));
			fds[1] = malloc(sizeof(file_s));
			if(fds[0] == NULL || fds[1] == NULL){
				fprintf(stderr, "malloc failed for fds[0/1]\n");
				exit(1);
			}
			fds[0]->fd = open(in_path,O_RDONLY);
			pthread_mutex_init(&(fds[0]->lock), NULL);
			fds[1]->fd = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, write_perms);
			pthread_mutex_init(&(fds[1]->lock), NULL);

			free(in_path);
			free(out_path);

			tid = malloc(sizeof(pthread_t) * max_threads);
			if(tid == NULL){
				fprintf(stderr, "malloc failed for tid!\n");
				exit(1);
			}

			args = malloc(sizeof(t_args*) * max_threads);
			if(args == NULL){
				fprintf(stderr, "malloc failed for args!\n");
				exit(1);
			}

			sem_init(&sem, 0, 0);

			for (i = 0; i < max_threads; i++){
				args[i] = malloc(sizeof(t_args));
				if(args[i] == NULL){
					fprintf(stderr, "malloc failed for args[%ld]!\n", i);
					exit(1);
				}
				args[i]->fds = fds;
				args[i]->max_t = max_threads;
				args[i]->sem = &sem;
			}

			wait_ts = malloc(sizeof(unsigned int) * max_threads);
			if(wait_ts == NULL){
				fprintf(stderr, "malloc failed for wait_ts!\n");
				exit(1);
			}

			pthread_mutex_lock(&(fds[0]->lock));
			for(i = 0; i < max_threads; i++){
				args[i]->t_id = i;
				wait_ts[i] = 0;
				active++;
				pthread_create(&(tid[i]), 0, thread_func, (void*) args[i]);
			}
			pthread_mutex_unlock(&(fds[0]->lock));
		
			for(i = 0; i < max_threads; i++){
				pthread_join(tid[i], NULL);
				free(args[i]);
			}
			free(wait_ts);
			free(args);
			free(tid);

			close(fds[0]->fd);
			close(fds[1]->fd);
			free(fds[0]);
			free(fds[1]);
			free(fds);
			ems_terminate();
			exit(0);
		}
	}
	if(pid > 0){
		for(i = 0; i < num_files; i++){
			pid_temp = wait(&ret);
			if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) > 0)){
				fprintf(stderr, "Child process %d returned error\n", pid_temp);
				exit(1);
			}
			printf("Process %d exited with state %d\n", pid_temp, ret);
		}
	}
	closedir(dir);
	exit(0);
}

void* thread_func(void *args){
	int temp;
	t_args *t = (t_args*) args;
	file_s **files = (file_s**) t->fds;
	unsigned int event_id, delay, thread_id, i, temp_delay;
	size_t num_rows, num_columns, num_coords;
	size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
	//pthread_mutex_t count_lock;
	
	//pthread_mutex_init(&count_lock, NULL);
	while (1) {
		fflush(NULL);
		pthread_mutex_lock(&(files[0]->lock));
		if (barrier){
			//getchar();
			if(barrier_count < active-1){
				barrier_count++;
				//fprintf(stderr, "Thread %ld got stuck in the barrier\n", t->t_id);
				pthread_mutex_unlock(&(files[0]->lock));
				sem_wait(t->sem);
				//pthread_mutex_lock(&(files[0]->lock));
				//fprintf(stderr, "Thread %ld got out of the barrier\n", t->t_id);
				continue;
			}
			else{
				barrier = 0;
				//fprintf(stderr, "Thread %ld unlocked the barrier\n", t->t_id);
			}
			while(barrier_count > 0){
				barrier_count--;
				sem_post(t->sem);
			}
		}
		//pthread_rwlock_wrlock(&wait_lock);
		if(wait_ts[t->t_id] > 0){
			temp_delay = wait_ts[t->t_id];
			wait_ts[t->t_id] = 0;
			//pthread_rwlock_unlock(&wait_lock);
			pthread_mutex_unlock(&(files[0]->lock));
			//printf("Waiting...\n");
			ems_wait(temp_delay);
			continue;
		}
		/*else
			pthread_rwlock_unlock(&wait_lock);*/
		
		switch (get_next(files[0]->fd)) {
			case CMD_CREATE:
				temp = parse_create(files[0]->fd, &event_id, &num_rows, &num_columns);
				//fprintf(stderr, "Created event %d\n", event_id);
				pthread_mutex_unlock(&(files[0]->lock));
				if (temp != 0) {
					fprintf(stderr, "Invalid command. See HELP for usage\n");
					continue;
				}
				//printf("Command CREATE: event %d\n", event_id);
				if (ems_create(event_id, num_rows, num_columns)) {
					fprintf(stderr, "Failed to create event\n");
				}

				break;

			case CMD_RESERVE:
				num_coords = parse_reserve(files[0]->fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
				pthread_mutex_unlock(&(files[0]->lock));
				//fprintf(stderr, "Reserve in event %d\n", event_id);
				if (num_coords == 0) {
					fprintf(stderr, "Invalid command. See HELP for usage\n");
					continue;
				}
				//printf("Command RESERVE: event %d\n", event_id);
				if (ems_reserve(event_id, num_coords, xs, ys)) {
					fprintf(stderr, "Failed to reserve seats\n");
				}

				break;

			case CMD_SHOW:
				temp = parse_show(files[0]->fd, &event_id);
				pthread_mutex_unlock(&(files[0]->lock));
				//fprintf(stderr, "Show event %d\n", event_id);
				if (temp != 0) {
					fprintf(stderr, "Invalid command. See HELP for usage\n");
					continue;
				}
				//printf("Command SHOW: event %d\n", event_id);
				pthread_mutex_lock(&(files[1]->lock));
				if (ems_show(event_id, files[1]->fd)) {
					fprintf(stderr, "Failed to show event\n");
				}
				pthread_mutex_unlock(&(files[1]->lock));

				break;

			case CMD_LIST_EVENTS:
				pthread_mutex_unlock(&(files[0]->lock));
				//fprintf(stderr, "Show events\n");
				//printf("Command LIST EVENTS\n");
				pthread_mutex_lock(&(files[1]->lock));
				if (ems_list_events(files[1]->fd)) {
					fprintf(stderr, "Failed to list events\n");
				}
				pthread_mutex_unlock(&(files[1]->lock));

				break;

			case CMD_WAIT:
				temp = parse_wait(files[0]->fd, &delay, &thread_id);
				//fprintf(stderr, "Wait thread %d\n", thread_id);
				//printf("Command WAIT\n");
				switch (temp){
					case -1:
						fprintf(stderr, "Invalid command. See HELP for usage\n");
						pthread_mutex_unlock(&(files[0]->lock));
						continue;
					case 0:
						if (delay > 0) {
							//fprintf(stderr, "Wait all threads\n");
							//pthread_rwlock_wrlock(&wait_lock);
							for(i=0;i<t->max_t;i++)
								wait_ts[i] += delay;
							//pthread_rwlock_unlock(&wait_lock);
						}
						break;
					case 1:
						if (delay > 0) {
							//fprintf(stderr, "Wait thread %d\n", thread_id);
							//pthread_rwlock_wrlock(&wait_lock);
							wait_ts[thread_id] += delay;
							//pthread_rwlock_unlock(&wait_lock);
						}
						break;
					default:
						break;
				}
				pthread_mutex_unlock(&(files[0]->lock));
				break;

			case CMD_INVALID:
				pthread_mutex_unlock(&(files[0]->lock));
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				break;

			case CMD_HELP:
				pthread_mutex_unlock(&(files[0]->lock));
				fprintf(stderr, "Available commands:\n"
						"  CREATE <event_id> <num_rows> <num_columns>\n"
						"  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
						"  SHOW <event_id>\n"
						"  LIST\n"
						"  WAIT <delay_ms> [thread_id]\n"
						"  BARRIER\n"
						"  HELP\n");

				break;

			case CMD_BARRIER:
				barrier = 1;
				barrier_count = 0;
				fprintf(stderr, "BARRIER...\n");
				pthread_mutex_unlock(&(files[0]->lock));
				break;
			case CMD_EMPTY:
				pthread_mutex_unlock(&(files[0]->lock));
				break;

			case EOC:
				active--;
				//fprintf(stderr, "Thread %ld reached it's end\n", t->t_id);
				/*if(active == 1 && barrier == 1){
					sem_post(t->sem);
					barrier = 0;
				}*/
				pthread_mutex_unlock(&(files[0]->lock));
				return NULL;
		}
	}
}