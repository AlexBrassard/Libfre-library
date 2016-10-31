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



/* test only. */
void *fre_thread_start(void* arg)
{
  fre_pmatch * temp = NULL;
  printf("I am your thread\n");

  return;
}

int main(int argc, char **argv)
{

  /* test */
  size_t tnum = 4;
  pthread_t threads[4];
  
  size_t i = 0;
  fre_pattern *freg_object = NULL;
  char pat_to_parse[FRE_MAX_PATTERN_LENGHT];

  if (argc < 2){
    fprintf(stderr, "%s: Usage:\n %s [pattern]\n\n", argv[0], argv[0]);
    return -1;
  }
  /*  if ((pat_to_parse = calloc(FRE_MAX_PATTERN_LENGHT, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    return -1;
    }*/
  
  /* 
   * This eliminates the "use of uninitialized values" 
   * in _strip_pattern() valgrind complained about.
   */
  memset(pat_to_parse, 0, 256);
  strncpy(pat_to_parse, argv[1], strlen(argv[1]));
  
  if ((freg_object = intern__fre__plp_parser(pat_to_parse)) == NULL) {
    intern__fre__errmesg("Intern__fre__plp_parser");
    return -1;
  }

  print_pattern_hook(freg_object);
  /* Use of an uninitialized value.
   * Complete _match_op() which does populate the thread's pmatch_table
   * and see if valgrind still cries. 
   
   print_ptable_hook();
  */
  

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
  /*  free (pat_to_parse);*/
  intern__fre__free_pattern(freg_object);
  return 0;
}
  
