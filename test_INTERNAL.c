/*
 *
 *  Libfre  -  internal functions test program. 
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "fre_internal.h"


int main(void)
{
  fre_backref *bref_object = NULL;


  if ((bref_object = intern__fre__init_bref_arr()) == NULL){
    intern__fre__errmesg("Intern__fre__init_bref_arr");
    return FRE_ERROR;
  }
  intern_line_test();
  if (bref_object != NULL){
    printf("bref_object initialized\n");
  }
  intern__fre__free_bref_arr(bref_object);


  return 0;
}
