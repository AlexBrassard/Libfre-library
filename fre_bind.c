/*
 *
 *
 *  Libfre's Public Interface.
 *  Version  0.600
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
#include "fre_internal_macros.h"
#include "fre_internal_errcodes.h"


/* Error message. */
void FRE_PERROR(char *funcname)  {
  intern__fre__errmesg(funcname);
}


int fre_bind(char *pattern,       /* The regex pattern. */
	     char *string,        /* The string to bind the pattern against. */
	     size_t string_size)  /* The size of string. (NOT THE LENGHT !) */
{
  /*
   * Parse the pattern using the _plp_parser.
   * Call the operation in the fre_pattern object returned by the parser.
   */
  size_t pattern_len = 0, string_len = 0;
  size_t offset_to_start = 0;              /* To keep _match_op's positions in line with the original string. */
  fre_pattern *freg_object = NULL;
  int retval = 0;

  if (!pattern || !string){
    errno = EINVAL;
    return FRE_ERROR;
  }
  /* 
   * Make sure both pattern and string fits the library's lenght limits.
   * If no NUL byte has been found until we reached lib-limit - 1 , fail hard.
   */
  pattern_len = strnlen(pattern, FRE_MAX_PATTERN_LENGHT);
  if (pattern[pattern_len] != '\0') {
    errno = FRE_PATRNTOOLONG;
    return FRE_ERROR;
  }
  string_len = strnlen(string, FRE_ARG_STRING_MAX_LENGHT);
  if (string[string_len] != '\0'){
    pthread_mutex_lock(&fre_stderr_mutex);
    fprintf(stderr, "Libfre takes input no longer than %d bytes (including the terminating NULL byte).\n\
Fragment or reduce the size of your input.\n\n",
	    FRE_ARG_STRING_MAX_LENGHT);
    pthread_mutex_unlock(&fre_stderr_mutex);
    errno = EOVERFLOW;
    return FRE_ERROR;
  }
  /* 
   * If there's a pattern saved in the pmatch-table and its the
   * same as the one our caller just passed in, use the fre_pattern
   * object sitting in the pmatch-table and skip parsing completely.
   */
  if (fre_pmatch_table->ls_pattern != NULL
      && fre_pmatch_table->ls_pattern[0] != '\0') {
    if (strcmp(fre_pmatch_table->ls_pattern, pattern) == 0){
      if (fre_pmatch_table->fre_saved_object == true){
	freg_object = fre_pmatch_table->ls_object;
	goto skip_plp_parser;
      }
    }
  }
  /* Parse the caller's pattern. */
  if ((freg_object = intern__fre__plp_parser(pattern)) == NULL){
    intern__fre__errmesg("_plp_parser: Failed to parse the given pattern");
    return FRE_ERROR;
  }
  /* 
   * Save a copy of the caller's pattern and a pointer to the fre_pattern object
   * given by the _plp_parser in the pmatch-table, in cases where our caller is binding
   * multiple strings against the same pattern.
   */
  if (SU_strcpy(fre_pmatch_table->ls_pattern, pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  fre_pmatch_table->ls_object = freg_object;

 skip_plp_parser:
  /* Execute the operation. will make it more fancy later. testing for now. */
  
  switch (freg_object->fre_op_flag){
  case MATCH :
    retval = intern__fre__match_op(string, freg_object, &offset_to_start);
    break;
  case SUBSTITUTE:
    retval = intern__fre__substitute_op(string, string_size, freg_object, &offset_to_start);
    break;
  case TRANSLITERATE:
    retval = intern__fre__transliterate_op(string, string_size, freg_object);
    break;
  default:
    /* If really we made it all the way here with an invalid operation just abort everything. */
    abort();
  }
  print_pattern_hook(freg_object); /* DEBUG ONLY */
  intern__fre__free_pattern(freg_object);
  freg_object = NULL;
  /* Check how the operation went. */
  if (retval == FRE_ERROR){
    errno = FRE_OPERROR;
  }

  return retval;
}
