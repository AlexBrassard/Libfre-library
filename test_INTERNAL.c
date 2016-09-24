/*
 *
 *  Libfre  -  internal functions test program. 
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "fre_internal.h"

#define pmatch_table (intern__fre__pmatch_location())

/* test only. */
void *fre_thread_start(void* arg)
{
  fre_pmatch * temp = NULL;
  printf("I am your thread\n");
  pmatch_table->fre_mod_global = true;
  return;
}

int main(void)
{

  /* test */
  size_t tnum = 4;
  pthread_t threads[4];
  
  size_t i = 0;
  fre_pattern *freg_object = NULL;
  char pat_to_parse[256]; 

  /* 
   * This eliminates the "use of uninitialized values" 
   * in _strip_pattern() valgrind complained about.
   */
  memset(pat_to_parse, 0, 256);
  strcpy(pat_to_parse, "s<(bob)><lol$1a$1>gi");
  
  if ((freg_object = intern__fre__plp_parser(pat_to_parse)) == NULL) {
    intern__fre__errmesg("Intern__fre__plp_parser");
    return -1;
  }

  print_pattern_hook(freg_object);
  
  pmatch_table->fre_mod_global = true;
  for (i = 0; i < tnum; i++){
    if (pthread_create(&threads[i], NULL, &fre_thread_start, NULL) != 0){
      perror("Pthread_create");
      return 0;
    }
  }
  for (i = 0; i < tnum; i++){
    if (pthread_join(threads[i], NULL) != 0){
      perror("Pthread_join");
      return 0;
    }
  }

  intern__fre__free_pattern(freg_object);
  return 0;
}
  
