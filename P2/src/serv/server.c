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

void *thread_func(void *);

// Global Variables
sem_t thread_sem;

int main(int argc, char *argv[])
{
	int incoming = -1, balance = 0;
	ssize_t ret = -1;
	char *in_path = NULL;
	char *buffer = NULL;
	t_args *new_client = NULL;
	pthread_t tid;

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

	buffer = malloc(sizeof(char) * (1 + 40 + 40));
	if (buffer == NULL)
	{
		fprintf(stderr, "[Err]: allocation of memory for incoming buffer failed\n");
		exit(EXIT_FAILURE);
	}

	// Open pipe for reading
	// This waits for someone to open it for writing
	incoming = open(in_path, O_RDONLY);
	if (incoming == -1)
	{
		fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sem_init(&thread_sem, 0, MAX_SESSION) != 0)
	{
		fprintf(stderr, "[ERR]: initialization the semaphore failed\n");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		ret = read(incoming, buffer, sizeof(char) * (1 + 40 + 40));
		switch (ret)
		{
		case -1:
			fprintf(stderr, "[Err]: invalid incoming message format\n");
			exit(EXIT_FAILURE);
			break;
		case 0:
			continue;
		case 40:
			if (buffer[0] != '1')
			{
				fprintf(stderr, "Wrong OP_CODE for this pipe\n");
				continue;
			}
			new_client = malloc(sizeof(t_args));
			new_client->in = open(buffer + 1, O_RDONLY);
			new_client->out = open(buffer + 1 + 40, O_WRONLY);
			sem_wait(&thread_sem);
			pthread_create(&tid, 0, thread_func, (void *)new_client);
			break;
		default:
			// FIXME
			break;
		}
		if (sem_getvalue(&thread_sem, &balance) != 0)
		{
			// FIXME
			fprintf(stderr, "\n");
		}
		else
		{
			if (balance == MAX_SESSION)
			{
				// FIXME
				/* code to close everything */
				break;
			}
		}
	}
	sem_destroy(&thread_sem);
	free(buffer);
	close(incoming);
	unlink(in_path);
	return 0;
}

void *thread_func(void *args)
{
	unsigned int event_id, quit = 0;
	int func_ret = 0;
	char OP_CODE;
	t_args *t = (t_args *)args;
	ssize_t ret = -1;
	size_t num_rows, num_cols, num_coords;
	size_t *xs = NULL, *ys = NULL;

	while (1)
	{
		ret = read(t->in, &OP_CODE, 1);
		if (ret == 0)
		{
			break;
		}

		if (ret == -1)
		{
			fprintf(stderr, "[ERR]: read from client pipe failed\n");
			exit(1);
		}

		switch (pipe_get_next(t->in))
		{
		case OP_CREATE:
			if (pipe_parse_create(t->in, &event_id, &num_rows, &num_cols) != 0)
			{
				fprintf(stderr, "Invalid usage of CREATE operation\n");
			}
			func_ret = ems_create(event_id, num_rows, num_cols);
			write(t->out, &func_ret, sizeof(int));
			if (func_ret)
			{
				fprintf(stderr, "Failed to create event\n");
			}
			break;
		case OP_RESERVE:
			num_coords = pipe_parse_reserve(t->in, &event_id, &xs, &ys);
			if (num_coords == 0)
			{
				fprintf(stderr, "Invalid usage of RESERVE operation\n");
			}
			func_ret = ems_reserve(event_id, num_coords, xs, ys);
			write(t->out, &func_ret, sizeof(int));
			if (func_ret)
			{
				fprintf(stderr, "Failed to reserve seats\n");
			}
			free(xs);
			free(ys);
			break;
		case OP_SHOW:
			if (pipe_parse_show(t->in, &event_id))
			{
				fprintf(stderr, "Invalid usage of SHOW operation\n");
			}
			func_ret = ems_show(event_id, t->out);
			if (func_ret)
			{
				fprintf(stderr, "Failed to show event\n");
			}
			break;
		case OP_LIST_EVENTS:
			func_ret = ems_list_events(t->out);
			if (func_ret)
			{
				fprintf(stderr, "Failed to list events\n");
			}
			break;
		case OP_QUIT:
			quit = 1;
			break;
		case OP_INVALID:
			break;
		}
		if (quit)
			break;
	}
	free(t);
	sem_post(&thread_sem);
	pthread_exit(0);
}