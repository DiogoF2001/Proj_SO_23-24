#include "pipe_parser.h"

// #include <limits.h>
#include <stdlib.h>
// #include <string.h>
#include <unistd.h>

enum OP pipe_get_next(int fd)
{
	char buf[16];
	if (read(fd, buf, 1) != 1)
	{
		return OP_INVALID;
	}

	switch (buf[0])
	{
	case '2':
		return OP_QUIT;
	case '3':
		return OP_CREATE;
	case '4':
		return OP_RESERVE;
	case '5':
		return OP_SHOW;
	case '6':
		return OP_LIST_EVENTS;
	default:
		return OP_INVALID;
	}
}

// DONE

int pipe_parse_create(int fd, unsigned int *event_id, size_t *num_rows, size_t *num_cols)
{
	size_t num;

	if (read(fd, event_id, sizeof(unsigned int)) != sizeof(unsigned int))
	{
		return 1;
	}

	if (read(fd, &num, sizeof(size_t)) != sizeof(size_t))
	{
		return 1;
	}
	*num_rows = (size_t)num;

	if (read(fd, &num, sizeof(size_t)) != sizeof(size_t))
	{
		return 1;
	}
	*num_cols = (size_t)num;

	return 0;
}

// FIXME

size_t pipe_parse_reserve(int fd, unsigned int *event_id, size_t **xs, size_t **ys)
{
	size_t num_seats = 0, val, i;

	if (read(fd, event_id, sizeof(unsigned int)) != sizeof(unsigned int))
	{
		return 0;
	}

	if (read(fd, &num_seats, sizeof(size_t)) != sizeof(size_t))
	{
		return 0;
	}

	*xs = malloc(sizeof(size_t) * num_seats);
	*ys = malloc(sizeof(size_t) * num_seats);

	if (*xs == NULL || *ys == NULL)
	{
		write(STDERR_FILENO, "[Err]: malloc failed in parse_reserve\n", sizeof("[Err]: malloc failed in parse_reserve\n"));
	}

	for (i = 0; i < num_seats; i++)
	{
		if (read(fd, &val, sizeof(size_t)) != sizeof(size_t))
		{
			return 0;
		}
		*xs[i] = val;
	}

	for (i = 0; i < num_seats; i++)
	{
		if (read(fd, &val, sizeof(size_t)) != sizeof(size_t))
		{
			return 0;
		}
		*ys[i] = val;
	}

	return num_seats;
}

// DONE

int pipe_parse_show(int fd, unsigned int *event_id)
{
	if (read(fd, event_id, sizeof(unsigned int)) != sizeof(unsigned int))
	{
		return 1;
	}
	return 0;
}
