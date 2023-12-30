#ifndef PIPE_PARSER_H
#define PIPE_PARSER_H

#include <stddef.h>

enum OP {
  OP_CREATE,
  OP_RESERVE,
  OP_SHOW,
  OP_LIST_EVENTS,
  OP_INVALID,
  OP_QUIT
};

/// Reads an OP_CODE and returns the corresponding operation.
/// @param fd File descriptor to read from.
/// @return The operation read.
enum OP pipe_get_next(int fd);

/// Parses a CREATE operation.
/// @param fd File descriptor to read from.
/// @param event_id Pointer to the variable to store the event ID in.
/// @param num_rows Pointer to the variable to store the number of rows in.
/// @param num_cols Pointer to the variable to store the number of columns in.
/// @return 0 if the operation was parsed successfully, 1 otherwise.
int pipe_parse_create(int fd, unsigned int *event_id, size_t *num_rows,
                 size_t *num_cols);

/// Parses a RESERVE operation.
/// @param fd File descriptor to read from.
/// @param event_id Pointer to the variable to store the event ID in.
/// @param xs Pointer to the array to store the X coordinates in.
/// @param ys Pointer to the array to store the Y coordinates in.
/// @return Number of coordinates read. 0 on failure.
size_t pipe_parse_reserve(int fd, unsigned int *event_id, size_t **xs, size_t **ys);

/// Parses a SHOW operation.
/// @param fd File descriptor to read from.
/// @param event_id Pointer to the variable to store the event ID in.
/// @return 0 if the operation was parsed successfully, 1 otherwise.
int pipe_parse_show(int fd, unsigned int *event_id);

#endif // PIPE_PARSER_H
