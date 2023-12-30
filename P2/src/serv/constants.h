#include <pthread.h>
#define MAX_SESSION 8

typedef struct thread_args
{
	int in;
	int out;
} t_args;
