/*
 *
 *  Libfre  -  Main internal functions.
 *  Version:   0.600
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "fre_internal.h"
#include "fre_internal_macros.h"
#include "fre_internal_errcodes.h"

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

#ifdef FRE_TOKEN
#undef FRE_TOKEN
#endif
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
      errno = FRE_UNKNOWNOP;
      intern__fre__errmesg("_plp_parser");
      goto errjmp;
    }
  default:
    /* 
     * If the first token is a punctuation mark, assume a match operation.
     * Don't increment token_ind, this token will be used as the pattern's delimiter.
     */
    if (ispunct(FRE_TOKEN)){
      freg_object->fre_op_flag = MATCH;
      break;
    }
    else {
      errno = FRE_UNKNOWNOP;
      intern__fre__errmesg("_plp_parser");
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
    errno = FRE_PATRNDELIM; /* Make sure errno is not set, else perror() will be called. */
    intern__fre__errmesg("_plp_parser");
    goto errjmp;
  }

  /* 
   * Strip a pattern from its Perl-like element,
   * split the "matching pattern" from the "substitute" pattern (if there's one).
   */
  if (intern__fre__split_pattern(pattern, freg_object, token_ind) != FRE_OP_SUCCESSFUL){
    intern__fre__errmesg("_split_pattern");
    goto errjmp;
  }
  
  /* 
   * Convert a stripped Perl-like pattern, that's not a transliteration operation,
   * into a POSIX ERE conformant pattern. 
   */
  if (freg_object->fre_op_flag != TRANSLITERATE){
    if (intern__fre__perl_to_posix(freg_object, 0) != FRE_OP_SUCCESSFUL){
      intern__fre__errmesg("_perl_to_posix");
      goto errjmp;
    }
    if (freg_object->fre_op_flag == SUBSTITUTE) {
      if (intern__fre__perl_to_posix(freg_object, 1) != FRE_OP_SUCCESSFUL){
	intern__fre__errmesg("_perl_to_posix");
	goto errjmp;
      }
    }
    /* Compile the modified matching pattern. */
    if (intern__fre__compile_pattern(freg_object) == FRE_ERROR){
      intern__fre__errmesg("_compile_pattern");
      goto errjmp;
    }
  }

  return freg_object; /* Success! */
  
 errjmp:
  if (freg_object != NULL)
    intern__fre__free_pattern(freg_object);
  return NULL;        /* Fail. */
  
} /* intern__fre__plp_parser() */


int intern__fre__match_op(char *string,                  /* The string to bind the pattern against. */
			  fre_pattern *freg_object,      /* The information gathered by the _plp_parser(). */
			  size_t *numof_tokens)           /* Number of tokens skiped by a previous global recursion. */
{
  size_t reg_c = 0;
  int i = 0;
  int replacement_len = 0;
  size_t string_len = strnlen(string, FRE_ARG_STRING_MAX_LENGHT);
  int match_ret = 0, match_op_ret = 0;
  regmatch_t regmatch_arr[FRE_MAX_SUB_MATCHES];
  char *string_copy = NULL;
  fre_smatch **stemp = NULL, **wtemp = NULL;
  
  if (!string || !freg_object || string_len++ >= FRE_ARG_STRING_MAX_LENGHT){
    errno = EINVAL;
    return FRE_ERROR;
  }
  if ((string_copy = calloc(string_len, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjmp;
  }
  if (SU_strcpy(string_copy, string, string_len) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjmp;
  }
  
  if ((match_ret = regexec(freg_object->comp_pattern, string_copy, FRE_MAX_SUB_MATCHES,
			   regmatch_arr, 0)) == FRE_ERROR){
    intern__fre__errmesg("Regexec");
    return FRE_ERROR;
  }
  /* Match successful. */
  if (match_ret == 0){
    /* Register positions of match/sub-matches now. */
    if (regmatch_arr[reg_c].rm_so != -1){
      fre_pmatch_table->lastop_retval = FRE_OP_SUCCESSFUL;
      fre_pmatch_table->whole_match[WM_IND]->bo = regmatch_arr[reg_c].rm_so + *numof_tokens;
      fre_pmatch_table->whole_match[WM_IND]->eo = regmatch_arr[reg_c].rm_eo + *numof_tokens;
      ++reg_c;
      while (reg_c < FRE_MAX_SUB_MATCHES && regmatch_arr[reg_c].rm_so != -1){
	fre_pmatch_table->sub_match[SM_IND]->bo = regmatch_arr[reg_c].rm_so + *numof_tokens;
	fre_pmatch_table->sub_match[SM_IND]->eo = regmatch_arr[reg_c].rm_eo + *numof_tokens;
	++reg_c;
	/* Extend the sub-match list if needed. */
	if (++SM_IND >= fre_pmatch_table->sm_size){
	  /* 0 == whole_match list, 1 == sub_match list. */
	  if (intern__fre__extend_ptable_list(1) == FRE_ERROR){
	    intern__fre__errmesg("_extend_ptable_list");
	    goto errjmp;
	  }
	}
      }
    }
    else {
      /* 
       * If the match is considered successful but regmatch[0] 
       * contains only -1s, we've got a problem. 
       * Long way of saying it can never happen or chaos and darkness will follow. 
       */
      errno = FRE_INVALBREF;
      goto errjmp;
    }
    /*The number of sub-matches per matches. */
    fre_pmatch_table->subm_per_match = reg_c - 1;

    /* 
     * Check for the presence of word boundaries now. 
     * Not sure yet where exactly to check boundaries in a substitute pattern.
     (I was wrong, substitute pattern MUST not be modified !).
     */
    if (freg_object->fre_match_op_bow == true || freg_object->fre_match_op_eow == true){
      match_ret = 0;
      FRE_CHECK_BOUNDARY(freg_object, string, numof_tokens, 0, &match_ret);
      if ((fre_pmatch_table->lastop_retval = match_ret) != FRE_OP_SUCCESSFUL){
	if (intern__fre__cut_match(string_copy, numof_tokens, string_len,
				   fre_pmatch_table->whole_match[WM_IND]->bo,
				   fre_pmatch_table->whole_match[WM_IND]->eo) == NULL){
	  intern__fre__errmesg("_cut_match");
	  goto errjmp;
	}
	/* Clear the current, unsuccessful match position from the pmatch_table. */
	FRE_CANCEL_CUR_MATCH();
	if (intern__fre__match_op(string_copy, freg_object, numof_tokens) == FRE_ERROR){
	  intern__fre__errmesg("_match_op");
	  goto errjmp;
	}
      }
    }
    
    if (freg_object->fre_match_op_bref == true){
      /* Decrement SM_IND by the number of sub-matches per matches, _insert_sm() will use it. */
      SM_IND -= fre_pmatch_table->subm_per_match;
      if ((replacement_len = intern__fre__insert_sm(freg_object, string, *numof_tokens, 0)) == FRE_ERROR){
	intern__fre__errmesg("_insert_sm");
	goto errjmp;
      }
      regfree(freg_object->comp_pattern);
      if (intern__fre__compile_pattern(freg_object) == FRE_ERROR){
	intern__fre__errmesg("_compile_pattern");
	goto errjmp;
      }
      /* Can the next statement overflow? */
      /* Only set fre_match_op_bref to false if there's no more backreference to insert. */
      freg_object->fre_match_op_bref = false;
      FRE_CANCEL_CUR_MATCH();
      if ((match_op_ret = intern__fre__match_op(string, freg_object, numof_tokens)) == FRE_ERROR){
	intern__fre__errmesg("_match_op");
	goto errjmp;
      }
      else if (match_op_ret == FRE_OP_SUCCESSFUL){
	fre_pmatch_table->lastop_retval = FRE_OP_SUCCESSFUL;
	freg_object->fre_match_op_bref = true;
      }
      else {
	fre_pmatch_table->lastop_retval = FRE_OP_UNSUCCESSFUL;
	/* Resets revelant fields of the ptable to -1.
	   Think about this twice, the current match was cleared before the recursive call to match_op. */
	FRE_CANCEL_ALL_MATCH();
      }
    }
    /* Extend the ->whole_match list if needed. */
    if (++WM_IND >= fre_pmatch_table->wm_size)
      if (intern__fre__extend_ptable_list(0) == FRE_ERROR){
	intern__fre__errmesg("_extend_ptable_list");
	goto errjmp;
      }
    /* Handle global operations. */
    if (freg_object->fre_mod_global == true){
      if (intern__fre__cut_match(string_copy, numof_tokens, string_len,
				 fre_pmatch_table->whole_match[WM_IND-1]->bo,
				 fre_pmatch_table->whole_match[WM_IND-1]->eo) == NULL){
	intern__fre__errmesg("_cut_match");
	goto errjmp;
      }
      if (strlen(string_copy) > 0){
	/* If the original pattern was changed with backrefs, restore it. */
	if (freg_object->fre_match_op_bref == true){
	  regfree(freg_object->comp_pattern);
	  if (SU_strcpy(freg_object->striped_pattern[0],
			freg_object->saved_pattern[0],
			FRE_MAX_PATTERN_LENGHT) == NULL){
	    intern__fre__errmesg("SU_strcpy");
	    goto errjmp;
	  }
	  if (intern__fre__compile_pattern(freg_object) == FRE_ERROR){
	    intern__fre__errmesg("_compile_pattern");
	    goto errjmp;
	  }
	}
	if (intern__fre__match_op(string_copy, freg_object, numof_tokens) == FRE_ERROR){
	  intern__fre__errmesg("_match_op");
	  goto errjmp;
	}
      }
    }
  }
  else {
    if (fre_pmatch_table->lastop_retval != FRE_OP_SUCCESSFUL)
      fre_pmatch_table->lastop_retval = FRE_OP_UNSUCCESSFUL;
  }
	 
  if (string_copy)
    free(string_copy);
  string_copy = NULL;
  return fre_pmatch_table->lastop_retval;

 errjmp:
  if (string_copy){
    free(string_copy);
    string_copy = NULL;
  }
  if (stemp){
    for (i = 0; i < (int)fre_pmatch_table->sm_size; i++)
      if (stemp[i]){
	free(stemp[i]);
	stemp[i] = NULL;
      }
    free(stemp);
    stemp = NULL;
  }
  if (wtemp){
    for (i = 0; i < (int)fre_pmatch_table->wm_size; i++)
      if(wtemp[i]){
	free(wtemp[i]);
	wtemp[i] = NULL;
      }
    free(wtemp);
    wtemp = NULL;
  }
  fre_pmatch_table->lastop_retval = FRE_ERROR;
  return FRE_ERROR;

} /* intern__fre__match_op() */


/* Execute a substitution operation. */
int intern__fre__substitute_op(char *string,
			       size_t string_size,
			       fre_pattern *freg_object,
			       size_t *offset_to_start)
{
  char *new_string = NULL;
  char *string_copy = NULL;   /* use the string directly?? */
  int match_ret = 0;
  int replacement_len = 0;
  int sumof_tokens = 0;
  int sumof_lenghts = 0;
  int replacement_ind = 0, string_ind = 0;
  int sp_ind = 0, ns_ind = 0;
  size_t new_string_len = strnlen(string, FRE_ARG_STRING_MAX_LENGHT);
  int numof_tokens = 0;
  size_t subm_per_match = fre_pmatch_table->subm_per_match;

  if (!string || !string_size
      || !freg_object || !offset_to_start) {
    errno = EINVAL;
    goto errjmp;
  }
  if ((match_ret = intern__fre__match_op(string, freg_object, offset_to_start)) == FRE_ERROR){
    intern__fre__errmesg("_match_op");
    goto errjmp;
  }
  else if (match_ret == FRE_OP_UNSUCCESSFUL){
    fre_pmatch_table->lastop_retval = FRE_OP_UNSUCCESSFUL; /* redundant */
    return FRE_OP_UNSUCCESSFUL;
  }
  /* Successful match. */
  else {
    WM_IND = 0; SM_IND = 0;
    if ((new_string = calloc(string_size, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      goto errjmp;
    }
    if ((string_copy = calloc(string_size, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      goto errjmp;
    }
    if(SU_strcpy(string_copy, string, string_size) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }

    while (string_copy[string_ind] != '\0'
	   && fre_pmatch_table->whole_match[WM_IND]->bo != -1){
      /* 
       * Add the sum of all replacement string and sub the number of tokens removed from the caller's
       * string to the index in ptable->wm->bo to find where in the modified string this index is at. 
       */
      if (string_ind == fre_pmatch_table->whole_match[WM_IND]->bo - sumof_tokens + sumof_lenghts){
	size_t temp_sumof_tokens = (size_t)sumof_tokens;
	if (intern__fre__cut_match(string_copy, &temp_sumof_tokens, string_size,
				   fre_pmatch_table->whole_match[WM_IND]->bo,
				   fre_pmatch_table->whole_match[WM_IND]->eo) == NULL){
	  intern__fre__errmesg("_cut_match");
	  goto errjmp;
	}
	sumof_tokens = (int)temp_sumof_tokens;
	/*sumof_tokens += numof_tokens;  can overflow. */
	if (freg_object->fre_subs_op_bref == true){
	  if (SU_strcpy(freg_object->striped_pattern[1], freg_object->saved_pattern[1], FRE_MAX_PATTERN_LENGHT) == NULL){
	    intern__fre__errmesg("SU_strcpy");
	    goto errjmp;
	  }
	  if ((replacement_len = intern__fre__insert_sm(freg_object, string,
							0, 1)) == FRE_ERROR){
	    intern__fre__errmesg("_insert_sm");
	    goto errjmp;
	  }
	}
	for (sp_ind = 0;
	     freg_object->striped_pattern[1][sp_ind] != '\0';
	     sp_ind++)
	  new_string[ns_ind++] = freg_object->striped_pattern[1][sp_ind];
	++WM_IND; /* Get next match positions. */
      }
      else {
	new_string[ns_ind++] = string_copy[string_ind++];
      }
      
    }
  }
  
  /* Make sure we leave no one behind. */
  while (string_copy[string_ind] != '\0')
    new_string[ns_ind++] = string_copy[string_ind++];
  new_string[ns_ind] = '\0';

  if (SU_strcpy(string, new_string, string_size) == NULL){
    intern__fre__errmesg("SU_strcpy");
    goto errjmp;
  }
  if (new_string){
    free(new_string);
    new_string = NULL;
  }
  if (string_copy){
    free(string_copy);
    string_copy = NULL;
  }
  return FRE_OP_SUCCESSFUL;

 errjmp:
  if (new_string){
    free(new_string);
    new_string = NULL;
  }
  if (string_copy){
    free(string_copy);
    string_copy = NULL;
  }
  fre_pmatch_table->lastop_retval = FRE_ERROR;
  return FRE_ERROR;
  

} /* intern__fre__substitute_op() */


/* Execute a transliteration operation (paired-character substitution). */
int intern__fre__transliterate_op(char *string,
				  size_t string_size,
				  fre_pattern *freg_object)
{

  size_t i = 0, j = 0, token_ind = 0;
  size_t sp_ind = 0;
  char **new_striped_p = NULL;
  char *new_string = NULL;

  if (!string || !freg_object || string_size == 0){
    errno = EINVAL;
    return FRE_ERROR;
  }

  if ((new_string = calloc(string_size, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    return FRE_ERROR;
  }
  if ((new_striped_p = malloc(2 * sizeof(char*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < 2; i++)
    if ((new_striped_p[i] = calloc(FRE_MAX_PATTERN_LENGHT, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      goto errjmp;
    }
 
  /*
   * Ranges are the only supported special characters in a
   * transliteration operation pattern. Look for and replace them.
   */
  while(sp_ind < 2){
    j = 0; i = 0;
    while(freg_object->striped_pattern[sp_ind][i] != '\0') {
      if (freg_object->striped_pattern[sp_ind][i] == '\\'
	  && freg_object->striped_pattern[sp_ind][i + 1] == 'd'){
	int low = '0', high = '9';
	FRE_INSERT_DASH_RANGE(new_striped_p[sp_ind], &j, low, high);
	i += 2; /* skip \d */
	continue;
      }
      else if (freg_object->striped_pattern[sp_ind][i] == '-'){
	--j; /* Since the first character has already been added to new_s_p. */
	FRE_INSERT_DASH_RANGE(new_striped_p[sp_ind], &j,
			      freg_object->striped_pattern[sp_ind][i - 1],
			      freg_object->striped_pattern[sp_ind][i + 1]);
	i += 2; /* skip -x */
	continue;
      }
      new_striped_p[sp_ind][j++] = freg_object->striped_pattern[sp_ind][i++];
    }
    if (SU_strcpy(freg_object->striped_pattern[sp_ind],
		  new_striped_p[sp_ind],
		  FRE_MAX_PATTERN_LENGHT) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }
    ++sp_ind;
  }
  /* 
   * If both patterns of a transliteration expression do not contain a matching
   * number of characters its a syntax error.
   */
  if (strlen(freg_object->striped_pattern[0]) !=
      strlen(freg_object->striped_pattern[1])){
    errno = FRE_INVALTRANSL;
    goto errjmp;
  }
      
  /* No need for these arrays anymore. */
  if (new_striped_p){
    for (i = 0; i < 2; i++){
      if (new_striped_p[i]){
	free(new_striped_p[i]);
	new_striped_p[i] = NULL;
      }
    }
    free(new_striped_p);
    new_striped_p = NULL;
  }

  /* Transliterate right here, right now ! */
  for (i = 0, token_ind = 0; string[i] != '\0'; i++){
    j = 0;
    while (freg_object->striped_pattern[0][j] != '\0') {
      if (string[i] == freg_object->striped_pattern[0][j]){
	new_string[token_ind++] = freg_object->striped_pattern[1][j];
	j = 0;
	++i;
	continue;
      }
      j++;
    }
    new_string[token_ind++] = string[i];
  }
  new_string[token_ind] = '\0';
  if (SU_strcpy(string, new_string, string_size) == NULL){
    intern__fre__errmesg("SU_strcpy");
    goto errjmp;
  }
  free(new_string);
  new_string = NULL;

  return FRE_OP_SUCCESSFUL;
  
 errjmp:
  if (new_striped_p){
    for (i = 0; i < 2; i++){
      if (new_striped_p[i]){
	free(new_striped_p[i]);
	new_striped_p[i] = NULL;
      }
    }
    free(new_striped_p);
    new_striped_p = NULL;
  }
  if (new_string){
    free(new_string);
    new_string = NULL;
  }
  return FRE_ERROR;
}
  


/* Vulnerable to 'use of uninitialized value'. Do not use in production code. */
void print_ptable_hook(void)
{
  size_t i = 0, n = 0;
  pthread_mutex_lock(&fre_stderr_mutex);
  fprintf(stderr, "fre_saved_object: %s\n", ((fre_pmatch_table->fre_saved_object == true) ? "true" : "false"));
  fprintf(stderr, "lastop_retval: %d\n", fre_pmatch_table->lastop_retval);
  fprintf(stderr, "ls_pattern: %s\n", ((fre_pmatch_table->ls_pattern[0] != '\0') ? fre_pmatch_table->ls_pattern : "NULL"));
  fprintf(stderr, "ls_object: %s\nWhole_match positions:\n",
	  ((fre_pmatch_table->ls_object) ? "Defined" : "NULL"));
  while (i < fre_pmatch_table->wm_size && fre_pmatch_table->whole_match[i]->bo != -1) {
    if (n++ == 3){
      fprintf(stderr, "\n");
      n = 0;
    }
    fprintf(stderr, "[%zu]->bo: %d    ->eo: %d    ", i,
	    fre_pmatch_table->whole_match[i]->bo,
	    fre_pmatch_table->whole_match[i]->eo);
    ++i;
  }
  fprintf(stderr, "\nSub_match positions:\n");
  i = 0; n = 0;
  while (i < fre_pmatch_table->sm_size && fre_pmatch_table->sub_match[i]->bo != -1){
    if (n++ == 2){
      fprintf(stderr, "\n");
      n = 0;
    }
    fprintf(stderr, "[%zu]->bo: %d    ->eo: %d    ", i,
	    fre_pmatch_table->sub_match[i]->bo,
	    fre_pmatch_table->sub_match[i]->eo);
    ++i;
  }
  fprintf(stderr, "\nwm_ind: %zu\nsm_ind: %zu\nwm_size: %zu\nsm_size: %zu\n",
	  fre_pmatch_table->wm_ind, fre_pmatch_table->sm_ind,
	  fre_pmatch_table->wm_size, fre_pmatch_table->sm_size);
  fprintf(stderr, "subm_per_match: %d\n", fre_pmatch_table->subm_per_match);
  fprintf(stderr,"\n\n");
  pthread_mutex_unlock(&fre_stderr_mutex);
}
