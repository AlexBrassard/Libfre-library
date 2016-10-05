/*
 *
 *
 *  Libfre's Public Interface.
 *  Version  0.010
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <fre.h>
#include "fre_internal.h"



int fre_bind(char *pattern,       /* The regex pattern. */
	     char *string)        /* The string to bind the pattern against. */
{
  /*
   * Parse the pattern using the _plp_parser.
   * Call the operation in the fre_pattern object returned by the parser.
   */
  size_t pattern_len = 0, string_len = 0;
  fre_pattern *freg_object = NULL;
  int retval = 0;

  if (pattern == NULL
      || string == NULL){
    errno = EINVAL;
    return FRE_ERROR;
  }
  if ((pattern_len = strnlen(pattern, SIZE_MAX - 1)) >= FRE_MAX_PATTERN_LENGHT) {
    pthread_mutex_lock(&fre_stderr_mutex);
    fprintf(stderr, "Libfre handles pattern no longer than 256 bytes (including the terminating NULL byte)\n\
This is to keep POSIX conformance.\n\n");
    pthread_mutex_unlock(&fre_stderr_mutex);
    errno = EOVERFLOW;
    return FRE_ERROR;
  }
  if ((string_len = strnlen(string, SIZE_MAX - 1)) >= FRE_ARG_STRING_MAX_LENGHT) {
    pthread_mutex_lock(&fre_stderr_mutex);
    fprintf(stderr, "Libfre takes input no longer than %d bytes (including the terminating NULL byte).\n \
Fragment or reduce the size of your input.\n\n",
	    FRE_ARG_STRING_MAX_LENGHT);
    pthread_mutex_unlock(&fre_stderr_mutex);
    errno = EOVERFLOW;
    return FRE_ERROR;
  }

  /* Parse the caller's pattern. */
  if ((freg_object = intern__fre__plp_parser(pattern)) == NULL){
    intern__fre__errmesg("_plp_parser: Failed to parse the given pattern");
    return FRE_ERROR;
  }


  /* Execute the operation. will make it more fancy later. testing for now. */
  
  switch (freg_object->fre_op_flag){
  case MATCH :
    retval = intern__fre__match_op(string, freg_object);
    break;
  case SUBSTITUTE:
    puts("Will do substitution operation");
    break;
  case TRANSLITERATE:
    puts("Will do transliteration operation");
    break;
  default:
    /* If really we made it all the way here with an invalid operation just abort everything. */
    abort();
  }
  intern__fre__free_pattern(freg_object);
  freg_object = NULL;
  /* Check how the operation went. */
  if (retval == FRE_ERROR){
    intern__fre__errmesg("_op_match: Failed to execute matching operation");
  }
  else if(retval == FRE_OP_SUCCESSFUL){
    puts("MATCH !!");
  }
  else if(retval == FRE_OP_UNSUCCESSFUL){
    puts("No match.");
  }
  
  return retval;
}
