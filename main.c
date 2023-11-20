#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

#define DIR_NAME "jobs"
#define IN_EXT ".jobs"
#define OUT_EXT ".out"

int main(int argc, char *argv[]) {
	unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
	DIR *dir;
	struct dirent *dp;
	int in_id = 0, out_id = 0;
	char *in_path, *out_path;
	size_t name_size;

	dir = opendir(DIR_NAME);
	if(dir == NULL){
		fprintf(stderr, "opendir failed on '%s'", DIR_NAME);
		return 1;
	}

	if (argc > 1) {
		char *endptr;
		unsigned long int delay = strtoul(argv[1], &endptr, 10);

		if (*endptr != '\0' || delay > UINT_MAX) {
		fprintf(stderr, "Invalid delay value or value too large\n");
		return 1;
		}

		state_access_delay_ms = (unsigned int) delay;
	}

	if (ems_init(state_access_delay_ms)) {
		fprintf(stderr, "Failed to initialize EMS\n");
		return 1;
	}

	for (;;){
		dp = readdir(dir);
		if(dp == NULL)
			break;
		if(strchr(dp->d_name, '.') == NULL || strcmp(strchr(dp->d_name, '.'), IN_EXT) != 0)
			continue;

		in_path = malloc(sizeof(char) * (strlen(DIR_NAME) + 1 + strlen(dp->d_name) + 1));
		if(in_path == NULL){
			fprintf(stderr, "malloc failed for in_path\n");
			return 1;
		}
		strcpy(in_path, DIR_NAME);
		strcat(in_path, "/");
		strcat(in_path, dp->d_name);
		printf("%s\n", in_path);
		in_id = open(in_path,O_RDONLY);
		
		name_size = strlen(dp->d_name) - strlen(strchr(dp->d_name, '.'));
		out_path = malloc(sizeof(char) * (strlen(DIR_NAME) + 1 + name_size + strlen(OUT_EXT) + 1));
		if(out_path == NULL){
			fprintf(stderr, "malloc failed for out_path\n");
			return 1;
		}
		strcpy(out_path, DIR_NAME);
		strcat(out_path, "/");
		strncat(out_path, dp->d_name,name_size);
		strcat(out_path, OUT_EXT);
		out_id = open(out_path, O_CREAT);

		printf("%d: %s\n", in_id, in_path);
		printf("%d: %s\n", out_id, out_path);
		getchar();

		while (1) {
			unsigned int event_id, delay;
			size_t num_rows, num_columns, num_coords;
			size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

			printf("> ");
			fflush(stdout);

			switch (get_next(in_id)) {
				case CMD_CREATE:
					if (parse_create(STDIN_FILENO, &event_id, &num_rows, &num_columns) != 0) {
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
					ems_terminate();
			return 0;
			}
		}
		
	}
	

	
}
