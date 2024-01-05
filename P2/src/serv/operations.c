#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "eventlist.h"

static struct EventList *event_list = NULL;
static unsigned int state_access_delay_ms = 0;
static pthread_rwlock_t *list_lock;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms)
{
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event *get_event_with_delay(unsigned int event_id)
{
  struct Event *ret;
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  pthread_rwlock_rdlock(list_lock);
  ret = get_event(event_list, event_id);
  pthread_rwlock_unlock(list_lock);

  return ret;
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int *get_seat_with_delay(struct Event *event, size_t index)
{
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event *event, size_t row, size_t col)
{
  return (row - 1) * event->cols + col - 1;
}

int ems_init(unsigned int delay_ms)
{
  if (event_list != NULL)
  {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  list_lock = malloc(sizeof(pthread_rwlock_t));
  if (list_lock == NULL)
  {
    fprintf(stderr, "Error allocating memory for list_lock\n");
    return 1;
  }
  pthread_rwlock_init(list_lock, NULL);

  return event_list == NULL;
}

int ems_terminate()
{
  if (event_list == NULL)
  {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
  pthread_rwlock_wrlock(list_lock);
  free_list(event_list);
  event_list = NULL;
  pthread_rwlock_unlock(list_lock);
  pthread_rwlock_destroy(list_lock);
  free(list_lock);
  list_lock = NULL;
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols)
{
  if (event_list == NULL)
  {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL)
  {
    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event *event = malloc(sizeof(struct Event));

  if (event == NULL)
  {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL)
  {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }
  pthread_rwlock_init(&(event->event_lock), NULL);

  for (size_t i = 0; i < num_rows * num_cols; i++)
  {
    event->data[i] = 0;
  }

  pthread_rwlock_wrlock(list_lock);

  if (append_to_list(event_list, event) != 0)
  {
    pthread_rwlock_unlock(list_lock);
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_destroy(&(event->event_lock));
    free(event->data);
    free(event);
    pthread_rwlock_unlock(list_lock);
    return 1;
  }
  pthread_rwlock_unlock(list_lock);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys)
{
  if (event_list == NULL)
  {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id);

  if (event == NULL)
  {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  pthread_rwlock_wrlock(&(event->event_lock));

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++)
  {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols)
    {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0)
    {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats)
  {
    event->reservations--;
    for (size_t j = 0; j < i; j++)
    {
      if (*get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) == 0)
        *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    pthread_rwlock_unlock(&(event->event_lock));
    return 1;
  }
  pthread_rwlock_unlock(&(event->event_lock));

  return 0;
}

int ems_show(unsigned int event_id, int fd)
{
  int ret = 1;
  if (event_list == NULL)
  {
    fprintf(stderr, "EMS state must be initialized\n");
    write(fd, &ret, sizeof(int));
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id);

  if (event == NULL)
  {
    fprintf(stderr, "Event not found\n");
    write(fd, &ret, sizeof(int));
    return 1;
  }

  pthread_rwlock_rdlock(&(event->event_lock));

  ret = 0;
  write(fd, &ret, sizeof(int));
  write(fd, &(event->rows), sizeof(size_t));
  write(fd, &(event->cols), sizeof(size_t));

  for (size_t i = 1; i <= event->rows; i++)
  {
    for (size_t j = 1; j <= event->cols; j++)
    {
      unsigned int *seat = get_seat_with_delay(event, seat_index(event, i, j));
      write(fd, seat, sizeof(unsigned int));
    }
  }
  pthread_rwlock_unlock(&(event->event_lock));

  return 0;
}

int ems_list_events(int fd)
{
  int ret = 1;
  size_t num_events = 0;

  pthread_rwlock_rdlock(list_lock);

  if (event_list == NULL)
  {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(list_lock);
    write(fd, &ret, sizeof(int));
    return 1;
  }

  if (event_list->head == NULL)
  {
    pthread_rwlock_unlock(list_lock);
    ret = 0;
    write(fd, &ret, sizeof(int));
    // write to pipe no events exist
    write(fd, &num_events, sizeof(size_t));
    return 0;
  }

  struct ListNode *current = event_list->head;
  while (current != NULL)
  {
    current = current->next;
    num_events++;
  }
  write(fd, &num_events, sizeof(size_t));

  current = event_list->head;
  while (current != NULL)
  {
    write(fd, &(current->event->id), sizeof(unsigned int));
    current = current->next;
  }

  pthread_rwlock_unlock(list_lock);

  return 0;
}
