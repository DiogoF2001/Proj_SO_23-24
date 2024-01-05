#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <pthread.h>

#define MAX_SESSION 	8
#define PROD_CONS_SIZE	20
#define STATE_ACCESS_DELAY_MS 10
#define MAX_RESERVATION_SIZE 256

typedef struct thread_args
{
	char req[40];
	char resp[40];
} t_args;

/*Function to call for new threads*/
void *thread_func(void *);

/*
*Signal handler routine for SIGUSR1 and SIGINT:
*	- for SIGINT, it only raises the flag
*	- for SIGUSR, it changes the flag to the opposite
*/
void signal_handler(int s);

#endif // CONSTANTS_H
