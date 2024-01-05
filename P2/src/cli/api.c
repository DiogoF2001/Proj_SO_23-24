#include "api.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "constants.h"

/*Global variables*/
int request, response, session_id;

int ems_setup(char const *req_pipe_path, char const *resp_pipe_path, char const *server_pipe_path)
{
  size_t i;
  if (unlink(req_pipe_path) != 0 && errno != ENOENT)
  {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", req_pipe_path,
            strerror(errno));
    return 1;
  }

  if (unlink(resp_pipe_path) != 0 && errno != ENOENT)
  {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", resp_pipe_path,
            strerror(errno));
    return 1;
  }

  if (mkfifo(req_pipe_path, 0640) != 0 || mkfifo(resp_pipe_path, 0640) != 0)
  {
    fprintf(stderr, "[ERR]: mkfifo failed\n");
    return 1;
  }

  int serv = open(server_pipe_path, O_WRONLY);
  if (serv == -1)
  {
    fprintf(stderr, "[ERR]: open failed\n");
    return 1;
  }

  char *source = malloc(sizeof(char) * (1 + 40 + 40));
  source[0] = '1';
  strncpy(source + 1, req_pipe_path, strlen(req_pipe_path));
  for (i = strlen(req_pipe_path) + 1; i < 40 + 1; i++)
  {
    source[i] = '\0';
  }

  strncpy(source + 1 + 40, resp_pipe_path, strlen(resp_pipe_path));
  for (i = strlen(resp_pipe_path) + 1 + 40; i < 1 + 40 + 40; i++)
  {
    source[i] = '\0';
  }

  if (write(serv, source, sizeof(char) * (1 + 40 + 40)) == -1)
  {
    return 1;
  }

  request = open(req_pipe_path, O_WRONLY);
  if (request == -1)
  {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }

  response = open(resp_pipe_path, O_RDONLY);
  if (response == -1)
  {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }

  read(response, &session_id, sizeof(int));

  free(source);
  close(serv);

  return 0;
}

int ems_quit(void)
{
  if (write(request, "2", sizeof(char) == -1))
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int) == -1))
  {
    return 1;
  }

  close(request);
  close(response);

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols)
{
  int ret;

  if (write(request, "3", sizeof(char)) == -1)
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int)) == -1)
  {
    return 1;
  }

  if (write(request, &event_id, sizeof(unsigned int)) != sizeof(unsigned int))
  {
    return 1;
  }

  if (write(request, &num_rows, sizeof(size_t)) != sizeof(size_t))
  {
    return 1;
  }

  if (write(request, &num_cols, sizeof(size_t)) != sizeof(size_t))
  {
    return 1;
  }

  if (read(response, &ret, sizeof(int)) == -1)
    return 1;

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys)
{
  int ret;
  size_t val, i;

  if (write(request, "4", sizeof(char)) == -1)
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int)) == -1)
  {
    return 1;
  }

  if (write(request, &event_id, sizeof(unsigned int)) != sizeof(unsigned int))
  {
    return 1;
  }

  if (write(request, &num_seats, sizeof(size_t)) != sizeof(size_t))
  {
    return 1;
  }

  for (i = 0; i < num_seats; i++)
  {
    val = xs[i];
    if (write(request, &val, sizeof(size_t)) != sizeof(size_t))
    {
      return 1;
    }
  }

  for (i = 0; i < num_seats; i++)
  {
    val = ys[i];
    if (write(request, &val, sizeof(size_t)) != sizeof(size_t))
    {
      return 1;
    }
  }

  if (read(response, &ret, sizeof(int)) == -1)
    return 1;

  return ret;
}

int ems_show(int out_fd, unsigned int event_id)
{
  int ret;
  unsigned int seat;
  size_t rows, cols, i, j;
  char temp[MAX_VALUE_UNSIGNED_INT];

  if (write(request, "5", sizeof(char)) == -1)
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int)) == -1)
  {
    return 1;
  }

  if (write(request, &event_id, sizeof(unsigned int)) != sizeof(unsigned int))
  {
    return 1;
  }

  if (read(response, &ret, sizeof(int)) == -1)
  {
    return 1;
  }

  if (ret == 1)
  {
    fprintf(stderr, "[ERR]: Event not found: %s\n", strerror(errno));
    return 1;
  }

  if (read(response, &rows, sizeof(size_t)) == -1)
  {
    return 1;
  }

  if (read(response, &cols, sizeof(size_t)) == -1)
  {
    return 1;
  }

  for (i = 0; i < rows; i++)
  {
    for (j = 0; j < cols; j++)
    {
      read(response, &seat, sizeof(seat));
      sprintf(temp,"%u",seat);
      write(out_fd, temp, sizeof(char) * strlen(temp));

      if (j < cols)
      {
        write(out_fd, " ", sizeof(char));
      }
    }

    write(out_fd, "\n", sizeof(char));
  }

  return 0;
}

int ems_list_events(int out_fd)
{
  int ret;
  size_t num_events, i;
  unsigned int event_id;
  char temp[MAX_VALUE_UNSIGNED_INT];

  if (write(request, "6", sizeof(char)) == -1)
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int)) == -1)
  {
    return 1;
  }

  if (read(response, &ret, sizeof(int)) == -1)
  {
    return 1;
  }

  if (ret == 1)
  {
    fprintf(stderr, "[ERR]: Event not found: %s\n", strerror(errno));
    return 1;
  }

  if (read(response, &num_events, sizeof(size_t)) == -1)
  {
    return 1;
  }

  if (num_events == 0)
  {
    write(out_fd, "No events.\n", sizeof(char) * strlen("No events.\n"));
    return 1;
  }

  for (i = 0; i < num_events; i++)
  { 
    read(response, &event_id, sizeof(event_id));
    write(out_fd, "Event: ", sizeof(char) * strlen("Event: "));
    sprintf(temp, "%u", event_id);
    write(out_fd, temp, sizeof(char) * strlen(temp));
    write(out_fd, "\n", sizeof(char));
  }
  return 0;
}
