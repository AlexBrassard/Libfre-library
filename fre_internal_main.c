/*
 *
 *  Libfre  -  Main internal functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#include "fre_internal.h"


/*
 * The Perl-like Pattern Parser.
 * Takes a pattern that has Perl-like syntax, converts it to a POSIX 
 * conformant pattern and output it in the form of a fre_pattern object, 
 * containing all informations relative to the pattern given by the caller.
 *
 * The public interface's fre_bind() is reponsible of untaining 
 * the pattern before calling on _plp_parser() with it.
 * 
 */
fre_pattern* intern__fre__plp_parser(char *pattern)
{
  size_t i = 0;
  size_t token_ind = 0;                 /* Index of the current token. */
  fre_pattern *freg_object = NULL;      /* The pattern to return to our caller. */


#define FRE_TOKEN pattern[token_ind]    /* Improves readability. */


  /* Request some memory for the caller's pattern. */
  if ((freg_object = intern__fre__init_pattern()) == NULL){
    intern__fre__errmesg("Intern__fre__init_pattern");
    return NULL;
  }

  /* Fetch the pattern's operation type. */
  switch(FRE_TOKEN){
  case 'm':
    freg_object->fre_op_flag = MATCH;
    ++token_ind;
    break;
  case 's':
    freg_object->fre_op_flag = SUBSTITUTE;
    ++token_ind;
    break;
  case 't':
    /* The next token must be 'r' (tr/), else it's an error. */
    ++token_ind;
    if (FRE_TOKEN == 'r') {
      freg_object->fre_op_flag = TRANSLITERATE;
      ++token_ind;
      break;
    }
    else {
      intern__fre__errmesg("Syntax error: Unknown operation in pattern");
      goto errjmp;
    }
  default:
    /* 
     * If the first token is a punctuation mark, assume a match operation.
     * This token will be used as the pattern's delimiter.
     */
    if (ispunct(FRE_TOKEN)){
      freg_object->fre_op_flag = MATCH;
      break;
    }
    else {
      intern__fre__errmesg("Syntax error: Unknown operation in pattern");
      goto errjmp;
    }
  }

  /* The current token should be a punctuation mark, the delimiter. */
  if (ispunct(FRE_TOKEN)) {
    freg_object->delimiter = FRE_TOKEN;
    /* Check if the token is to be a paired-type delimiter. */
    for (i = 0; FRE_PAIRED_O_DELIMITERS[i] != '\0'; i++){
      /* 
       * If it is, register its closing delimiter token in the fre_pattern. 
       * and raise up the appropriate flag.
       */
      if (FRE_TOKEN == FRE_PAIRED_O_DELIMITERS[i]){
	freg_object->fre_paired_delimiters = true;
	freg_object->c_delimiter = FRE_PAIRED_C_DELIMITERS[i];
      }
    }
    ++token_ind;
  }
  /* Syntax error. */
  else {
    errno = 0; /* Make sure errno is not set, else perror() will be called. */
    intern__fre__errmesg("Syntax error: Missing delimiter in pattern");
    goto errjmp;
  }

  /* 
   * Strip a pattern from its Perl-like element,
   * split the "matching pattern" and "substitute" pattern (if there's one).
   */

  
  return freg_object; /* Success! */
  
 errjmp:
  if (freg_object != NULL)
    intern__fre__free_pattern(freg_object);
  return NULL;        /* Fail. */
  
} /* intern__fre__plp_parser() */