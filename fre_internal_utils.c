/*
 *
 *
 *  Libfre  -  Internal utilities.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <regex.h>
#include <errno.h>

#include "fre_internal.h"


/* Allocate memory for a single fre_pattern object. 
fre_pattern* intern__fre__init_pattern(void)
{
  return NULL; 
}*/

/* Allocate memory for arrays holding possible back-reference(s) position(s). */
fre_backref* intern__fre__init_bref_arr(void)
{
  fre_backref *to_init = NULL;

  if ((to_init = malloc(sizeof(fre_backref))) == NULL) {
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  if ((to_init->in_pattern = calloc(FRE_MAX_SUB_MATCHES, sizeof(int))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjump;
  }
  if ((to_init->in_substitute = calloc(FRE_MAX_SUB_MATCHES, sizeof(int))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjump;
  }
  to_init->in_pattern_c = 0;
  to_init->in_substitute_c = 0;
  return to_init;

 errjump:
  if (to_init != NULL){
    if (to_init->in_pattern != NULL){
      free(to_init->in_pattern);
      to_init->in_pattern = NULL;
    }
    if (to_init->in_substitute != NULL){
      free(to_init->in_substitute);
      to_init->in_substitute = NULL;
    }
    free(to_init);
    to_init = NULL;
  }
  return NULL;
}

void intern__fre__free_bref_arr(fre_backref *to_free)
{
  /* Return right away if we're being passed a NULL argument. */
  if (to_free == NULL){
    return;
  }
  if (to_free->in_pattern != NULL){
    free(to_free->in_pattern);
    to_free->in_pattern = NULL;
  }
  if (to_free->in_substitute != NULL){
    free(to_free->in_substitute);
    to_free->in_substitute = NULL;
  }
  free(to_free);
  to_free = NULL;
  return;
}


void intern_line_test(void)
{
  printf("This is just a test\n");
  return;
}
  
