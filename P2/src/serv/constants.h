#include <pthread.h>

#define MAX_SESSION 	8
#define PROD_CONS_SIZE	20

typedef struct thread_args
{
	char req[40];
	char resp[40];
} t_args;

