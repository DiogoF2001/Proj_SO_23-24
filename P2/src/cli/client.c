#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "constants.h"
#include "api.h"
#include "parser.h"

#define MAX_RESERVATION_SIZE 256
#define STATE_ACCESS_DELAY_US 500000 // 500ms
#define MAX_JOB_FILE_NAME_SIZE 256
#define MAX_SESSION_COUNT 8

int main(int argc, char *argv[])
{
	int temp;
	char *out_path;
	int in, out;
	size_t name_size;
	mode_t write_perms;
	unsigned int event_id, delay, thread_id;
	size_t num_rows, num_columns, num_coords;
	size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

	write_perms = 00400 | 00200 | 00040 | 00020 | 00004;

	if (argc < 5)
	{
		fprintf(stderr, "Usage: %s <request pipe path> <response pipe path> <server pipe path> <.jobs file path>\n",
				argv[0]);
		exit(1);
	}

	if (ems_setup(argv[1], argv[2], argv[3]))
	{
		fprintf(stderr, "Failed to set up EMS\n");
		return 1;
	}

	const char *dot = strrchr(argv[4], '.');
	if (dot == NULL || dot == argv[4] || strlen(dot) != 5 || strcmp(dot, IN_EXT) ||
		strlen(argv[4]) > MAX_JOB_FILE_NAME_SIZE)
	{
		fprintf(stderr, "The provided .jobs file path is not valid. Path: %s\n", argv[1]);
		exit(1);
	}

	name_size = strlen(argv[4]) - strlen(strchr(argv[4], '.'));
	out_path = malloc(sizeof(char) * (name_size + strlen(OUT_EXT) + 1));
	if (out_path == NULL)
	{
		fprintf(stderr, "malloc failed for out_path\n");
		exit(1);
	}
	strcpy(out_path, argv[4]);
	strcpy(strchr(out_path, '.'), OUT_EXT);
	printf("%s\n%s\n",argv[4], out_path);

	in = open(argv[4], O_RDONLY);
	if (in == -1) {
    	fprintf(stderr, "Failed to open input file. Path: %s\n", argv[4]);
    	return 1;
  	}

	out = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, write_perms);
	if (out == -1) {
    	fprintf(stderr, "Failed to open output file. Path: %s\n", out_path);
    	return 1;
  	}

	free(out_path);


	while (1) {
		fflush(NULL);
		switch (get_next(in))
		{
		case CMD_CREATE:
			temp = parse_create(in, &event_id, &num_rows, &num_columns);
			if (temp != 0)
			{
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				continue;
			}
			if (ems_create(event_id, num_rows, num_columns))
			{
				fprintf(stderr, "Failed to create event\n");
			}

			break;

		case CMD_RESERVE:
			num_coords = parse_reserve(in, MAX_RESERVATION_SIZE, &event_id, xs, ys);
			if (num_coords == 0)
			{
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				continue;
			}
			if (ems_reserve(event_id, num_coords, xs, ys))
			{
				fprintf(stderr, "Failed to reserve seats\n");
			}

			break;

		case CMD_SHOW:
			temp = parse_show(in, &event_id);
			if (temp != 0)
			{
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				continue;
			}
			if (ems_show(out, event_id))
			{
				fprintf(stderr, "Failed to show event\n");
			}

			break;

		case CMD_LIST_EVENTS:
			if (ems_list_events(out))
			{
				fprintf(stderr, "Failed to list events\n");
			}

			break;

		case CMD_WAIT:
			temp = parse_wait(in, &delay, &thread_id);
			switch (temp)
			{
			case -1:
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				continue;
			default:
				sleep(delay);
				break;
			}
			break;

		case CMD_INVALID:
			fprintf(stderr, "Invalid command. See HELP for usage\n");
			break;

		case CMD_HELP:
			fprintf(stderr, "Available commands:\n"
							"  CREATE <event_id> <num_rows> <num_columns>\n"
							"  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
							"  SHOW <event_id>\n"
							"  LIST\n"
							"  WAIT <delay_ms> [thread_id]\n"
							"  BARRIER\n"
							"  HELP\n");

			break;

		case CMD_EMPTY:
			break;

		case EOC:
			close(in);
			close(out);
			ems_quit();
			return 0;
		}
	}
}
