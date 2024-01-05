#include <pthread.h>
#include <semaphore.h>

#define MAX_RESERVATION_SIZE 256
#define STATE_ACCESS_DELAY_MS 10
#define MAX_VALUE_UNSIGNED_INT 10
#define IN_EXT ".jobs"
#define OUT_EXT ".out"

typedef struct file_struct
{
	int fd;
	pthread_mutex_t lock;
} file_s;

typedef struct thread_ids
{
	pthread_t *ids;
	pthread_rwlock_t lock;
} t_ids;

typedef struct thread_args
{
	file_s **fds;
	sem_t *sem;
	long unsigned int t_id;
	long unsigned int max_t;
} t_args;

void *thread_func(void *);