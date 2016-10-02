/*
 *
 *  Libfre  -  Main internal functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "fre_internal.h"




/*
 * The Perl-like Pattern Parser.
 * Takes a pattern that has Perl-like syntax, converts it to a POSIX 
 * conformant pattern and output it in the form of a fre_pattern object, 
 * containing all informations relative to the pattern given by the caller.
 *
 * The public interface's fre_bind() is reponsible of untainting 
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
       * If it is, register its closing delimiter token in the fre_pattern, 
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
  if (intern__fre__strip_pattern(pattern, freg_object, token_ind) != FRE_OP_SUCCESSFUL){
    intern__fre__errmesg("Intern__fre__strip_pattern");
    goto errjmp;
  }
  
  /* Convert a stripped Perl-like pattern into a POSIX ERE conformant pattern. */
  if (intern__fre__perl_to_posix(freg_object) != FRE_OP_SUCCESSFUL){
    intern__fre__errmesg("Intern__fre__perl_to_posix");
    goto errjmp;
  }

  /* Compile the modified pattern. */
  if (intern__fre__compile_pattern(freg_object) == FRE_ERROR){
    intern__fre__errmesg("Intern__fre__compile_pattern");
    goto errjmp;
  }
  return freg_object; /* Success! */
  
 errjmp:
  if (freg_object != NULL)
    intern__fre__free_pattern(freg_object);
  return NULL;        /* Fail. */
  
} /* intern__fre__plp_parser() */


int intern__fre__match_op(char *string,                  /* The string to bind the pattern against. */
			  fre_pattern *freg_object)      /* The information gathered by the _plp_parser(). */
{
  int    reg_ret = 0;                                    /* To hold regexec's return value. */
  size_t i = 0;
  size_t string_lenght = 0;
  size_t sub_match_ind = 0;
  size_t string_copy_bo = 0;                             /* String_copy's begining of match. */
  size_t offset_to_start = 0;                            /* 
							  * Offset from the current begining of match 
							  * down to the begining of string. 
							  */
  regmatch_t regmatch_array[FRE_MAX_SUB_MATCHES];        /* To fetch the sub matches' positions. */
  char *string_copy = NULL;                              /* Our copy of the caller's string. */
  bool REGEXEC_MATCH = false;                              /* 
							  * Since regexec might be called multiple times within
							  * a single _match_op() invocation, we use this flag to
							  * determine a successful return or not.
							  */

  /* Using INT_MAX (FRE_ARG_STRING_MAX_LENGHT) since sub-matches position are saved as integers. */
  if ((string_lenght = strlen(string) >= (FRE_ARG_STRING_MAX_LENGHT - 1))){
    errno = EOVERFLOW;
    return FRE_ERROR;
  }
  else {
    if ((string_copy = calloc(FRE_ARG_STRING_MAX_LENGHT, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      return FRE_ERROR;
    }
    if (SU_strcpy(string_copy, string, FRE_ARG_STRING_MAX_LENGHT) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }
    /*   memcpy(string_copy, string, string_lenght);
    string_copy[string_lenght + 1] = '\0';*/
  }

  /*
   * To avoid looping throught every arrays of every node of a thread's 
   * pmatch_table, we garantee that ->whole_match->*o is set to a valid index or
   * -1 when no matches are found. ->sub_array[] will contain either valid indexes or 
   * garbage values if whole_match->*o is set to -1.
   */
  for (i = 0; i < FRE_MAX_SUB_MATCHES; i++){
    fre_pmatch_table->whole_match->bo = -1;
    fre_pmatch_table->whole_match->eo = -1;
    intern__fre__pmatch_location(fre_pmatch_table->next_match);
  }
  /* Go back to the list's headnode. */
  intern__fre__pmatch_location(fre_pmatch_table->headnode);
  

  if ((reg_ret = regexec(freg_object->comp_pattern, string_copy, FRE_MAX_SUB_MATCHES, regmatch_array, 0)) != 0) {
    if (reg_ret != REG_NOMATCH){
      intern__fre__errmesg("Regexec");
      goto errjmp;
    }
    /* break ? */
  }
    
  while (1) {
    /* Match. */
    REGEXEC_MATCH = true;
    /* Save the position of the complete match. */
    fre_pmatch_table->whole_match->bo = regmatch_array[0].rm_so;
    fre_pmatch_table->whole_match->eo = regmatch_array[0].rm_eo;
    i = 1;
    while (regmatch_array[i].rm_so != -1){
      fre_pmatch_table->sub_match[sub_match_ind]->bo = (regmatch_array[i].rm_so + offset_to_start);
      fre_pmatch_table->sub_match[sub_match_ind]->eo = (regmatch_array[i].rm_eo + offset_to_start);
      sub_match_ind++;
      i++;
    }
    break;
  }
  return FRE_OP_SUCCESSFUL;
  
 errjmp:
  if (string_copy != NULL){
    free(string_copy);
    string_copy = NULL;
  }
  return FRE_ERROR;
}



void print_ptable_hook(void)
{
  size_t i = 0;

  fprintf(stderr, "This thread's pmatch_table:\n\n%12s: %d\n%14s: %d\n",
	  "Whole_match->bo", fre_pmatch_table->whole_match->bo,
	  "->eo", fre_pmatch_table->whole_match->eo);
  for (i = 0;
       i < FRE_MAX_SUB_MATCHES || fre_pmatch_table->sub_match[i]->bo != -1;
       i++){
    fprintf(stderr, "Sub_match[%zu]->bo: %d\n             ->eo:%d\n",
	    i, fre_pmatch_table->sub_match[i]->bo, fre_pmatch_table->sub_match[i]->eo);
  }

  printf("\n");
}
