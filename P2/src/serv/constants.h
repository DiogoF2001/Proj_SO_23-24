#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <pthread.h>

#define MAX_SESSION 	8
#define PROD_CONS_SIZE	20
#define STATE_ACCESS_DELAY_MS 10

typedef struct thread_args
{
	char req[40];
	char resp[40];
} t_args;

#endif // CONSTANTS_H
