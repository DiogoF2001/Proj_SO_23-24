#include "api.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

/*Variaveis Globais*/
int request, response;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", req_pipe_path,
                strerror(errno));
        exit(1);
    }

  if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", resp_pipe_path,
                strerror(errno));
        exit(1);
    }

  if (mkfifo(req_pipe_path,0640) != 0 || mkfifo(resp_pipe_path, 0640) !=0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s");
    return 1;
  }

  request = open(req_pipe_path,O_WRONLY);
	if (request == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return 1;
    }

	response= open(resp_pipe_path,O_RDONLY);
	if (response == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return 1;
    }

  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  close(request);
  close(response);

  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)

  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
