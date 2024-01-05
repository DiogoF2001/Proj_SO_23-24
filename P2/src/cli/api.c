#include "api.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

/*Variaveis Globais*/
int request, response, session_id;

int ems_setup(char const *req_pipe_path, char const *resp_pipe_path, char const *server_pipe_path)
{
  // TODO: create pipes and connect to the server
  if (unlink(req_pipe_path) != 0 && errno != ENOENT)
  {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", req_pipe_path,
            strerror(errno));
    exit(1);
  }

  if (unlink(resp_pipe_path) != 0 && errno != ENOENT)
  {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", resp_pipe_path,
            strerror(errno));
    exit(1);
  }

  if (mkfifo(req_pipe_path, 0640) != 0 || mkfifo(resp_pipe_path, 0640) != 0)
  {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", stderror(errno));
    return 1;
  }

  int serv = open(server_pipe_path, O_WRONLY);
  if (serv == -1)
  {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }

  char *source = malloc(sizeof(char) * (1 + 40 + 40));
  source[0] = '1';
  strncy(source + 1, req_pipe_path, strlen(req_pipe_path));
  for (int i = strlen(req_pipe_path) + 1; i < 40 + 1; i++)
  {
    source[i] = '\0';
  }

  strncy(source + 1 + 40, resp_pipe_path, strlen(resp_pipe_path));
  for (int i = strlen(resp_pipe_path) + 1 + 40; i < 1 + 40 + 40; i++)
  {
    source[i] = '\0';
  }

  if (write(serv, source, sizeof(source) == -1))
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
  // TODO: close pipes
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
  // TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  int ret;

  if (write(request, "3", sizeof(char) == -1))
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int) == -1))
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
  // TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  int ret;
  size_t val;

  if (write(request, "4", sizeof(char) == -1))
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int) == -1))
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

  for (int i = 0; i < num_seats; i++)
  {
    val = xs[i];
    if (write(request, &val, sizeof(size_t)) != sizeof(size_t))
    {
      return 1;
    }
  }

  for (int i = 0; i < num_seats; i++)
  {
    val = ys[i];
    if (write(request, &val, sizeof(size_t)) != sizeof(size_t))
    {
      return 1;
    }
  }

  if (read(response, &ret, sizeof(int)) == -1)
    return 1;

  if (ret == num_seats)
    return 1;

  return 0;
}

int ems_show(int out_fd, unsigned int event_id)
{
  // TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  int ret;
  unsigned int seat;
  size_t rows, cols;

  if (write(request, "5", sizeof(char) == -1))
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int) == -1))
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

  for (size_t i = 0; i < rows; i++)
  {
    for (size_t j = 0; j < cols; j++)
    {
      read(response, &seat, sizeof(seat));
      write(out_fd, &seat, sizeof(seat));

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
  // TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  int ret;
  size_t num_events;
  unsigned int event_id;

  if (write(request, "6", sizeof(char) == -1))
  {
    return 1;
  }

  if (write(request, &session_id, sizeof(int) == -1))
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

  if (read(response, &num_events, sizeof(int)) == -1)
  {
    return 1;
  }

  if (num_events == 0)
  {
    write(out_fd, "No events.", sizeof("No events.\n"));
    return 1;
  }

  for (size_t i = 0; i < num_events; i++)
  {
    read(response, &event_id, sizeof(event_id));
    write(out_fd, "Event: ", sizeof("Event: "));
    write(out_fd, i, sizeof(int));
    write(out_fd, "\n", sizeof(char));
  }
  return 0;
}