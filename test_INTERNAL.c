/*
 *
 *  Libfre  -  internal functions test program. 
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "fre_internal.h"

/* 
 * These are only test, most of the functions won't be used directly.
 * Let the system free resources on early error exits. 
 */
int main(void)
{
  size_t i = 0;
  fre_pmatch  *pmatch_node = NULL;
  fre_pmatch  *headnode = NULL;
  fre_backref *bref_object = NULL;
  fre_pattern *pattern_test = NULL;

  /* Internal arrays used to keep positions of submatches within their pattern. */
  if ((bref_object = intern__fre__init_bref_arr()) == NULL){
    intern__fre__errmesg("Intern__fre__init_bref_arr");
    return FRE_ERROR;
  }

  /* Single node of a pmatch-table. */
  if ((pmatch_node = intern__fre__init_pmatch_node(NULL)) == NULL){
    intern__fre__errmesg("Intern__fre__init_pmatch_node");
    return FRE_ERROR;
  }
  headnode = pmatch_node;
  for (i = 0; i < FRE_MAX_SUB_MATCHES; i++){
    if ((pmatch_node->next_match = intern__fre__init_pmatch_node(headnode)) == NULL){
      intern__fre__errmesg("Intern__fre__init_pmatch_node");
      return FRE_ERROR;
    }
    pmatch_node = pmatch_node->next_match;
  }

  if ((pattern_test = intern__fre__init_pattern()) == NULL){
    intern__fre__errmesg("Intern__fre__init_pattern");
    return FRE_ERROR;
  }

  /* Cleanup. */
  intern__fre__free_bref_arr(bref_object);
  intern__fre__free_ptable(pmatch_node->headnode);
  intern__fre__free_pattern(pattern_test);

  return 0;
}
