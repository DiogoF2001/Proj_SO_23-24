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

int main(int argc, char *argv[]) {
	unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
	DIR *dir;
	struct dirent *dp;
	int in_id = 0, out_id = 1, end_of_file = 0, ret = 1;
	char *in_path, *out_path;
	size_t name_size;
	mode_t write_perms;
	pid_t pid;
	unsigned long int max_proc, max_threads, *num_files = NULL, i;

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

	if (ems_init(state_access_delay_ms)) {
		fprintf(stderr, "Failed to initialize EMS\n");
		exit(1);
	}

	num_files = malloc(sizeof(unsigned long int));
	if(num_files == NULL){
		fprintf(stderr, "malloc failed for num_files!\n");
		exit(1);
	}

	*num_files = 0;

	write_perms = 00400 | 00200 | 00040 | 00020 | 00004;

	for (;;){
		dp = readdir(dir);
		if(dp == NULL)
			break;
		if(strchr(dp->d_name, '.') == NULL || strcmp(strchr(dp->d_name, '.'), IN_EXT) != 0)
			continue;
		if(*num_files >= max_proc){
			//printf("Reached the limit\n");
			wait(&ret);
			if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) > 0)){
				fprintf(stderr, "Child process returned error\n");
				exit(1);
			}
		}
		else
			(*num_files)++;
		//printf("Num_files: %ld\n", *num_files);
		pid = fork();
		if(pid == 0){
			in_path = malloc(sizeof(char) * (strlen(argv[1]) + 1 + strlen(dp->d_name) + 1));
			if(in_path == NULL){
				fprintf(stderr, "malloc failed for in_path\n");
				exit(1);
			}
			strcpy(in_path, argv[1]);
			strcat(in_path, "/");
			strcat(in_path, dp->d_name);
			in_id = open(in_path,O_RDONLY);
			
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
			out_id = open(out_path, O_CREAT | O_WRONLY, write_perms);

			free(in_path);
			free(out_path);

			while (1) {
				unsigned int event_id, delay;
				size_t num_rows, num_columns, num_coords;
				size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

				switch (get_next(in_id)) {
					case CMD_CREATE:
						if (parse_create(in_id, &event_id, &num_rows, &num_columns) != 0) {
							fprintf(stderr, "Invalid command. See HELP for usage\n");
							continue;
						}

						if (ems_create(event_id, num_rows, num_columns)) {
							fprintf(stderr, "Failed to create event\n");
						}

						break;

					case CMD_RESERVE:
						num_coords =
							parse_reserve(in_id, MAX_RESERVATION_SIZE, &event_id, xs, ys);

						if (num_coords == 0) {
							fprintf(stderr, "Invalid command. See HELP for usage\n");
							continue;
						}

						if (ems_reserve(event_id, num_coords, xs, ys)) {
							fprintf(stderr, "Failed to reserve seats\n");
						}

						break;

					case CMD_SHOW:
						if (parse_show(in_id, &event_id) != 0) {
							fprintf(stderr, "Invalid command. See HELP for usage\n");
							continue;
						}

						if (ems_show(event_id, out_id)) {
							fprintf(stderr, "Failed to show event\n");
						}

						break;

					case CMD_LIST_EVENTS:
						if (ems_list_events(out_id)) {
							fprintf(stderr, "Failed to list events\n");
						}

						break;

					case CMD_WAIT:
						if (parse_wait(in_id, &delay, NULL) ==
							-1) { // thread_id is not implemented
							fprintf(stderr, "Invalid command. See HELP for usage\n");
							continue;
						}

						if (delay > 0) {
							printf("Waiting...\n");
							ems_wait(delay);
						}

						break;

					case CMD_INVALID:
						fprintf(stderr, "Invalid command. See HELP for usage\n");
						break;

					case CMD_HELP:
						printf("Available commands:\n"
								"  CREATE <event_id> <num_rows> <num_columns>\n"
								"  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
								"  SHOW <event_id>\n"
								"  LIST\n"
								"  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
								"  BARRIER\n"                     // Not implemented
								"  HELP\n");

						break;

					case CMD_BARRIER: // Not implemented
					case CMD_EMPTY:
						break;

					case EOC:
						end_of_file = 1;
				}
				if(end_of_file){
					end_of_file = 0;
					break;
				}
			}
			close(in_id);
			close(out_id);
			exit(0);
		}
	}
	if(pid > 0){
		for(i = 0; i < *num_files; i++){
			wait(&ret);
			if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) > 0)){
				fprintf(stderr, "Child process returned error\n");
				exit(1);
			}
		}
	}
	free(num_files);
	ems_terminate();
	closedir(dir);
	exit(0);
}
