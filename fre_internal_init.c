/*
 *
 *
 *  Libfre  -  Initialization and termination routines.
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "fre_internal.h"


pthread_mutex_t fre_stderr_mutex;

int __attribute__ ((constructor)) intern__fre__lib_init(void)
{
  /* Initialize a mutex to serialize all stderr messages. */
  if (pthread_mutex_init(&fre_stderr_mutex, NULL) != 0){
    perror("Pthread_mutex_init");
    return FRE_ERROR;
  }

  return FRE_OP_SUCCESSFUL;
}

void __attribute__ ((destructor)) intern__fre__lib_finit(void)
{
  /* Deallocate stderr's mutex. */
  if (pthread_mutex_destroy(&fre_stderr_mutex) != 0){
    perror("Pthread_mutex_destroy");
    abort();
  }
}

