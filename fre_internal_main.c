/*
 *
 *  Libfre  -  Main internal functions.
 *  Version:   0.010
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "fre_internal.h"


extern pthread_mutex_t fre_stderr_mutex;

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
			  fre_pattern *freg_object,      /* The information gathered by the _plp_parser(). */
			  size_t *offset_to_start)       /* To keep string positions alignment in global operations. */
{
  bool bref_flag_wastrue = freg_object->fre_match_op_bref;
  int reg_retval = 0;
  char *string_copy = NULL;
  /*  size_t offset_to_start = 0;                             To keep string position alignment in global operations. */
  size_t string_len = 0;
  size_t i = 0, sm_ind = 0;
  regmatch_t regmatch_arr[FRE_MAX_SUB_MATCHES];


  if (string == NULL
      || freg_object == NULL){
    errno = EINVAL;
    return FRE_ERROR;
  }
  
  string_len = strnlen(string, FRE_ARG_STRING_MAX_LENGHT);
  string_len++; /* 1 for the NUL byte. */
  if ((string_copy = calloc(string_len, sizeof(char))) == NULL) {
    intern__fre__errmesg("Calloc");
    return FRE_ERROR;
  }
  if (SU_strcpy(string_copy, string, string_len) == NULL){
    intern__fre__errmesg("SU_strcpy");
    goto errjmp;
  }
  
  if ((reg_retval = regexec(freg_object->comp_pattern, string, FRE_MAX_SUB_MATCHES,
			    regmatch_arr, 0)) != 0) {
    if (reg_retval != REG_NOMATCH) {
      intern__fre__errmesg("Regexec");
      goto errjmp;
    }
    if (fre_pmatch_table->last_op_ret_val != FRE_OP_SUCCESSFUL)
      fre_pmatch_table->last_op_ret_val = FRE_OP_UNSUCCESSFUL;
    goto skip_match; /* Above the return statement. */
  }

  /* Match! */
  fre_pmatch_table->last_op_ret_val = FRE_OP_SUCCESSFUL;
  if (regmatch_arr[i].rm_so != -1){
    fre_pmatch_table->whole_match->bo = regmatch_arr[i].rm_so + *offset_to_start;
    fre_pmatch_table->whole_match->eo = regmatch_arr[i].rm_eo + *offset_to_start;
  }
  ++i;
  while(regmatch_arr[i].rm_so != -1){
    fre_pmatch_table->sub_match[sm_ind]->bo = regmatch_arr[i].rm_so + *offset_to_start;
    fre_pmatch_table->sub_match[sm_ind]->eo = regmatch_arr[i].rm_eo + *offset_to_start;
    ++sm_ind;
    ++i;
  }
  if (freg_object->fre_match_op_bref == true){
    if (intern__fre__insert_sm(freg_object, string_copy, 0) == NULL){
      intern__fre__errmesg("Intern__fre__insert_sm");
      goto errjmp;
    }
    freg_object->fre_match_op_bref = false;
    regfree(freg_object->comp_pattern);
    if (intern__fre__compile_pattern(freg_object) == FRE_ERROR){
      intern__fre__errmesg("Intern__fre__compile_pattern");
      goto errjmp;
    }
    if (intern__fre__match_op(string_copy, freg_object, offset_to_start) == FRE_ERROR){
      intern__fre__errmesg("Intern__fre__match_op");
      goto errjmp;
    }
  }
  /* Check if the matching operation has global scope. */
  if (freg_object->fre_mod_global == true){
    if (bref_flag_wastrue == true){
      /* Restore ->striped_pattern[0] (Saved in ->striped_pattern[2]) */
      if (SU_strcpy(freg_object->striped_pattern[0], freg_object->saved_pattern[0], FRE_MAX_PATTERN_LENGHT) == NULL){
	intern__fre__errmesg("SU_strcpy");
	goto errjmp;
      }
    }
    freg_object->fre_match_op_bref = bref_flag_wastrue;
    /* Remove whatever matched from our own copy of the caller's string and recurse. */
    if (intern__fre__cut_match(string_copy, offset_to_start, string_len,
			       fre_pmatch_table->whole_match->bo, fre_pmatch_table->whole_match->eo) == NULL){
      intern__fre__errmesg("Intern__fre__cut_match");
      goto errjmp;
    }
    intern__fre__pmatch_location(fre_pmatch_table->next_match);
    if (intern__fre__match_op(string_copy, freg_object, offset_to_start) == FRE_ERROR){
      intern__fre__errmesg("Intern__fre__match_op");
      goto errjmp;
    }
  }
 skip_match:
  if (string_copy)
    free(string_copy);
  string_copy = NULL;
  /* Restore the pmatch_table's headnode. */
  intern__fre__pmatch_location(fre_pmatch_table->headnode);
  
  return fre_pmatch_table->last_op_ret_val;

 errjmp:
  if (string_copy)
    free(string_copy);
  string_copy = NULL;

  return FRE_ERROR;
} /* intern__fre__match_op() */


/* Execute a substitution operation. */
int intern__fre__substitute_op(char *string,
			       size_t string_size,
			       fre_pattern *freg_object,
			       size_t *offset_to_start)
{

  int match_ret = 0, i = 0;
  char *new_string = NULL;
  size_t new_string_lenght = 0;
  size_t j = 0, ns_ind = 0, sp_ind = 0;
  

  if (string == NULL
      || freg_object == NULL){
    errno = EINVAL;
    return FRE_ERROR;
  }
  new_string_lenght = strnlen(string, FRE_ARG_STRING_MAX_LENGHT);
  
  if ((match_ret = intern__fre__match_op(string, freg_object, offset_to_start)) == FRE_ERROR) {
    intern__fre__errmesg("Intern__fre__match_op");
    goto errjmp;
  }
  /* 
   * Give new_string the maximum amount of memory argument string are allowed
   * making sure we don't overflow when doing the substitutions. 
   */
  if ((new_string = calloc(FRE_ARG_STRING_MAX_LENGHT, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    return FRE_ERROR;
  }

  /* Match ! */
  if (match_ret == FRE_OP_SUCCESSFUL) {
    /* Replace back-reference(s) if there's any. */
    if (freg_object->fre_subs_op_bref == true){
      if (intern__fre__insert_sm(freg_object, string, 1) == NULL){
	intern__fre__errmesg("Intern__fre__insert_sm");
	goto errjmp;
      }
    }
    /* 
     * 'i' is cast to a size_t here only to remove the compiler warning.
     * Note that 'i' will never be < 0 and > FRE_ARG_STRING_MAX_LENGHT which 
     * is exactly INT_MAX.
     */
    while (string[i] != '\0'
	   && ((size_t)i) < new_string_lenght){
      if (i == fre_pmatch_table->whole_match->bo) {
	/* Make sure there's enough room in "string" to hold the substitution. */
	if ((new_string_lenght += (fre_pmatch_table->whole_match->eo - fre_pmatch_table->whole_match->bo))
	    >= (string_size - 1)) {
	  errno = EOVERFLOW;
	  intern__fre__errmesg("Intern__fre__substitute_op");
	  goto errjmp;
	}

	/* Substitute now. */
	sp_ind = 0;
	while(freg_object->striped_pattern[1][sp_ind] != '\0')
	  new_string[ns_ind++] = freg_object->striped_pattern[1][sp_ind++];

	/* Get the next match if there's one. */
	if (fre_pmatch_table->next_match != NULL
	    && fre_pmatch_table->next_match->whole_match->bo != -1){
	  intern__fre__pmatch_location(fre_pmatch_table->next_match);
	  /* Restore the substitute pattern to a clean state. */
	  if (SU_strcpy(freg_object->striped_pattern[1], freg_object->saved_pattern[1], FRE_MAX_PATTERN_LENGHT) == NULL){
	    intern__fre__errmesg("SU_strcpy");
	    goto errjmp;
	  }
	  /* Replace the new backrefs if any. */
	  if (intern__fre__insert_sm(freg_object, string, 1) == NULL){
	    intern__fre__errmesg("Intern__fre__insert_sm");
	    goto errjmp;
	  }
	}
	i++;
	continue;
      }
      new_string[ns_ind++] = string[i++];
    }
    if (SU_strcpy(string, new_string, string_size) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }
    free(new_string);
    new_string = NULL;
    return (fre_pmatch_table->last_op_ret_val = FRE_OP_SUCCESSFUL);  
  }
  /* No match. */
  else {
    if (new_string){
      free(new_string);
      new_string = NULL;
    }
    return (fre_pmatch_table->last_op_ret_val = FRE_OP_UNSUCCESSFUL);
  }

 errjmp:
  if (new_string){
    free(new_string);
    new_string = NULL;
  }
  return FRE_ERROR;

} /* intern__fre__substitute_op() */
    









  



void print_ptable_hook(void)
{
  size_t i = 0;
  size_t node_num = 0;
  pthread_mutex_lock(&fre_stderr_mutex);
 top:
  if (fre_pmatch_table->whole_match->bo != -1){
    fprintf(stderr, "[[Pmatch_table: Node %zu]]:\n%12s: %d\n%14s: %d\n", node_num++,
	    "Whole_match->bo", fre_pmatch_table->whole_match->bo,
	    "->eo", fre_pmatch_table->whole_match->eo);
    for (i = 0;
	 i < FRE_MAX_SUB_MATCHES;
	 i++){
      if (fre_pmatch_table->sub_match[i]->bo == -1){
	break;
      }
      fprintf(stderr, "Sub_match[%zu]->bo: %d\n             ->eo:%d\n",
	      i, fre_pmatch_table->sub_match[i]->bo, fre_pmatch_table->sub_match[i]->eo);
    }
    fprintf(stderr,"\n");
    if (fre_pmatch_table->next_match != NULL) {
      intern__fre__pmatch_location(fre_pmatch_table->next_match);
      goto top;
    }
  }
  fprintf(stderr,"\n\n");
  pthread_mutex_unlock(&fre_stderr_mutex);
}
