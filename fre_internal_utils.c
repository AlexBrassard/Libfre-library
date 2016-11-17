
/*
 *
 *
 *  Libfre  -  Internal utilities.
 *  Version:   0.600
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#include "fre_internal.h"
#include "fre_internal_macros.h"


extern fre_headnodes *fre_headnode_table;

/** Regex Parser Utility Functions. **/


/* 
 * Copy source into destination of size n (including the trailing NUL byte).
 * Note, SU_strcpy() always clears the buffer before
 * copying results into it. 
 */
void *SU_strcpy(char *dest, char *src, size_t n)
{
  size_t src_s = 0;
  
  if (dest != NULL                /* We need an already initialized buffer. */
      && src != NULL              /* We need a valid, non-empty source. */
      && src[0] != '\0'
      && n > 0                    /* Destination buffer's size must be bigger than 0, */
      && n < (SIZE_MAX - 1))      /* and smaller than it's type size - 1. */
    ; /* Valid input. */
  else {
    errno = EINVAL;
    return NULL;
  }
  
  /* Look out for a NULL byte, never past SIZE_MAX - 1. */
  src_s = strnlen(src, SIZE_MAX - 1);
  if (src_s > n - 1){ /* -1, we always add a NULL byte at the end of string. */
    errno = EOVERFLOW;
    fprintf(stderr,"%s: Source must be at most destination[n - 1]\n\n", __func__);
    dest = NULL;
    return dest;
  }
  
  memset(dest, 0, n);
  memcpy(dest, src, src_s);
  
  return dest;
}


int intern__fre__compile_pattern(fre_pattern *freg_object)
{
  if (regcomp(freg_object->comp_pattern,
	      freg_object->striped_pattern[0],
	      (freg_object->fre_mod_icase == true) ? REG_ICASE : 0 |
	      (freg_object->fre_mod_newline == true) ? 0 : REG_NEWLINE |
	      REG_EXTENDED) != 0){
    intern__fre__errmesg("Regcomp");
    return FRE_ERROR;
  }
  freg_object->fre_p1_compiled = true;
  return FRE_OP_SUCCESSFUL;
}


/* 
 * Strip a pattern from all its Perl-like elements.
 * Separates "matching" and "substitute" patterns of a 
 * substitution or transliteration operation and register the
 * substitute pattern's backreference positions here.
 * The matching pattern's backreference positions are handled by _perl_to_posix().
 */
int intern__fre__split_pattern(char *pattern,
			       fre_pattern *freg_object,
			       size_t token_ind)
{
  size_t spa_tos = 0;                      /* ->striped_pattern[0|1]'s tos values. */
  size_t spa_ind = 0;                      /* 0: matching pattern ind; 1: substitute pattern ind. */
  size_t delimiter_pairs = 1;              /* Count of pairs of paired-delimiter seen.*/
  size_t numof_delimiters = 1;             /* Number of delimiters seen in pattern. */
  size_t numof_tokens = 0;                 /* Amount of tokens seen since [0]: begining of string [>0]: since previous bref. */

#ifdef FRE_TOKEN
# undef FRE_TOKEN
#endif
#define FRE_TOKEN pattern[token_ind]

  while(1){
    if (FRE_TOKEN == '\0'){
      if (freg_object->fre_paired_delimiters == true && delimiter_pairs == 0){	break; }
      else if (freg_object->fre_op_flag == MATCH
	       && numof_delimiters == FRE_EXPECTED_M_OP_DELIMITER) { break; }
      else if (numof_delimiters == FRE_EXPECTED_ST_OP_DELIMITER
	       && freg_object->fre_op_flag != MATCH) { break; }
      else{ /* Syntax error: Missing closing delimiter. */
	errno = 0; intern__fre__errmesg("Pattern missing closing delimiter");
	errno = ENODATA;
	return FRE_ERROR;
      }
    }
    ++numof_tokens;

    /* Handle some escape sequences here (instead of protecting them so they reach _perl_to_posix()) */
    if (FRE_TOKEN == '\\') {
      if (pattern[token_ind+1] == 'Q'&& spa_ind == 0){
	FRE_QUOTE_TOKENS(freg_object, pattern, &token_ind,
			 &spa_tos, strlen(freg_object->striped_pattern[spa_ind]));
      }
      else if (pattern[token_ind+1] == freg_object->delimiter
	       || (freg_object->fre_paired_delimiters == true
		   && pattern[token_ind+1] == freg_object->c_delimiter)){
	FRE_PUSH(pattern[++token_ind], freg_object->striped_pattern[spa_ind], &spa_tos);
	++token_ind;
	continue;
      }
    }
    
    if (FRE_TOKEN == freg_object->delimiter){
      if (freg_object->fre_paired_delimiters == true){
	if (delimiter_pairs++ > 0){
	  FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	}
	else {
	  /* Begining a new pattern. spa_ind was incremented when we hit a ->c_delimiter. */
	  spa_tos = 0;
	  numof_tokens = 0;
	}
      }
      else { /* non-pair-typed delimiters */
	FRE_PUSH('\0', freg_object->striped_pattern[spa_ind], &spa_tos);
	++numof_delimiters;
	++spa_ind;
	if ((spa_ind == 1 && freg_object->fre_op_flag == MATCH)
	    || (spa_ind > 1 && freg_object->fre_op_flag != MATCH)){
	  ++token_ind;
	  FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	  continue;
	}
	else {
	  numof_tokens = 0;
	  spa_tos = 0;
	}
      }
    }

    else if (FRE_TOKEN == freg_object->c_delimiter){
      if (delimiter_pairs == 0){
	/* Syntax error. */
	errno = 0; intern__fre__errmesg("Invalid sequence of pattern delimiters");
	errno = EINVAL;
	return FRE_ERROR;
      }
      if (--delimiter_pairs > 0){
	FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
      }
      else {
	FRE_PUSH('\0', freg_object->striped_pattern[spa_ind], &spa_tos);
	if (freg_object->fre_op_flag == MATCH){
	  ++token_ind;
	  FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	  continue;
	}
	else{
	  if (++spa_ind > 1) {
	    ++token_ind;
	    FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	    continue;
	  }
	  /* Make sure the next token is an opening delimiter. */
	  if (pattern[token_ind + 1] != freg_object->delimiter) {
	    /* Syntax error. */
	    errno = 0; intern__fre__errmesg("Invalid character between opening and closing pattern delimiter");
	    errno = EINVAL;
	    return FRE_ERROR;
	  }
	}
      }
    }
    else {
      FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
    }

    ++token_ind; /* Get next token. */
  }
  return FRE_OP_SUCCESSFUL;

} /* intern__fre__split_pattern() */


/* 
 * Convert any non-POSIX, Perl-like elements into
 * POSIX supported constructs.
 */
int intern__fre__perl_to_posix(fre_pattern *freg_object,
			       size_t is_sub){
  bool   fre_do_not_push = false;     /* When true, tokens aren't pushed to new_pattern. */
  size_t in_bracket_exp = 0;          /* ++ each time we find a '[', -- each time we find  a ']'. */
  size_t in_bracket_count = 0;        /* Some meta-chars must be at a specific position in a bracket expression to be valid. */
  size_t numof_tokens = 0;            /* Used to help register back-reference positions. */
  size_t token_ind = 0;
  size_t new_pattern_tos = 0;              /* new_pattern's top of stack. */
  size_t new_pattern_len = strnlen(freg_object->striped_pattern[0], FRE_MAX_PATTERN_LENGHT-1);
  char   new_pattern[FRE_MAX_PATTERN_LENGHT]; /* The array used to build the new pattern. */

#ifdef FRE_TOKEN
#undef FRE_TOKEN
#endif
#define FRE_TOKEN freg_object->striped_pattern[is_sub][token_ind]

  if (!freg_object || (is_sub != 0 && is_sub != 1)){
    errno = EINVAL;
    return FRE_ERROR;
  }
  memset(new_pattern, 0, FRE_MAX_PATTERN_LENGHT);
  while (1){
    if (FRE_TOKEN == '\0'){
      if (in_bracket_exp > 0) {
	errno = 0; intern__fre__errmesg("Unterminated '[' or '[^' expression");
	errno = ENODATA;
	return FRE_ERROR;
      }
      FRE_PUSH('\0', new_pattern, &new_pattern_tos);
      break;
    }
    ++numof_tokens;

    if (FRE_TOKEN == '[') in_bracket_exp++;
    else if (FRE_TOKEN == ']') {
      /* 
       * Directly push the token on the new_pattern's stack if it's not in a bracket expression or
       * if a closing bracket is the first character after the leading bracket or
       * the second char after the leading bracket if the first char is a caret.
       */
      if ((in_bracket_exp == 0)
	  || (in_bracket_count == 0)
	  || (in_bracket_count == 1 && freg_object->striped_pattern[is_sub][token_ind-1] == '^'))
	goto push_token;
      else {
	if (--in_bracket_exp == 0) {
	  in_bracket_count = 0;
	}
      }
    }
    if (in_bracket_exp > 0) in_bracket_count++;
    /* Handle backreferences. */
    if ((FRE_TOKEN == '\\' || FRE_TOKEN == '$')
	&& (isdigit(freg_object->striped_pattern[is_sub][token_ind +1]))
	&& in_bracket_exp == 0){
      --numof_tokens;
      fre_do_not_push = true;
      if (is_sub){
	/* Conditional expression is useless, numof_tokens == new_pattern_tos when substitute_c == 0 */
	FRE_HANDLE_BREF(freg_object->striped_pattern[is_sub],
			&token_ind,
			(freg_object->backref_pos->in_substitute_c == 0) ? new_pattern_tos : numof_tokens,
			is_sub, freg_object);
      }
      else{
	FRE_HANDLE_BREF(freg_object->striped_pattern[is_sub],
			&token_ind,
			(freg_object->backref_pos->in_pattern_c == 0) ? new_pattern_tos : numof_tokens,
			is_sub, freg_object);
      }
      numof_tokens = 1;
      continue;
    }
    /* Never go any further with a subtitute pattern. */
    else if (is_sub == 1) goto push_token;
    /* Handle escape sequences. */
    else if (FRE_TOKEN == '\\' && in_bracket_exp == 0) {
      FRE_CERTIFY_ESC_SEQ(is_sub, &token_ind, new_pattern, &new_pattern_tos,
			  &new_pattern_len, freg_object);
      continue;
    }
    /* Handle comments. */
    if (freg_object->fre_mod_ext == true && in_bracket_exp == 0) {
      if (isspace(FRE_TOKEN)) {
	while(isspace(FRE_TOKEN)) ++token_ind;
	continue; /* Must avoid incrementing token_ind once more. */
      }
      if (FRE_TOKEN == '#') {
	FRE_SKIP_COMMENTS(freg_object->striped_pattern[is_sub],
			  &new_pattern_len,
			  &token_ind);
	if (errno){
	  intern__fre__errmesg("Fre_skip_comments");
	  return FRE_ERROR;
	}
	continue; /* Must avoid incrementing token_ind once more. */
      }
    }
    /* Just a regular token. */
    push_token:
    /*    if (fre_do_not_push == false)*/
    FRE_PUSH(FRE_TOKEN, new_pattern, &new_pattern_tos);
    
    ++token_ind; /* Get next token. */
  }
  /* 
   * Copy new_pattern to saved_pattern[] then if do_not_push was set to true, 
   * NULL terminate new_pattern at the index of the first found backreference and then
   * copy new_pattern to ->striped_pattern[].
   */
  if (SU_strcpy(freg_object->saved_pattern[is_sub], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  if (fre_do_not_push == true && !is_sub){
    new_pattern[freg_object->backref_pos->in_pattern[0]] = '\0';
  }
  if (SU_strcpy(freg_object->striped_pattern[is_sub], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  return FRE_OP_SUCCESSFUL;
  
} /* intern__fre__perl_to_posix() */


/* Insert sub-match(es) into the given pattern. */
int intern__fre__insert_sm(fre_pattern *freg_object,      /* The object used throughout the library. */
			   char *string,                  /* The string to match. */
			   int numof_tokens,              /* Number of tokens skiped by a global operation. */
			   size_t is_sub)
{
  bool added_bref = false;  /* True the first time we insert a backref. */
  int string_ind = 0, sp_ind = 0, np_ind = 0, in_array_ind = 0, i = 0;
  int sm_count = 0, inserted_count = 0;
  int *in_array = ((!is_sub) ? freg_object->backref_pos->in_pattern : freg_object->backref_pos->in_substitute);
  int first_elem_pos = in_array[in_array_ind];
  int next_elem_pos = 0;
  char new_pattern[FRE_MAX_PATTERN_LENGHT];

  if (!freg_object || !string || is_sub > 1){
    errno = EINVAL;
    return FRE_ERROR;
  }
  memset(new_pattern, 0, FRE_MAX_PATTERN_LENGHT);
  while (sp_ind < FRE_MAX_PATTERN_LENGHT) {
    long subm_ind = ((!is_sub)? freg_object->backref_pos->p_sm_number[sm_count]
		     : freg_object->backref_pos->s_sm_number[sm_count]);
    if (in_array[freg_object->bref_to_insert] == -1
	|| ((first_elem_pos + next_elem_pos == sp_ind) && (added_bref == true))) break;
    if (freg_object->bref_to_insert != 0)
      next_elem_pos = in_array[freg_object->bref_to_insert];
    if (sp_ind == first_elem_pos + next_elem_pos){
      if (!is_sub) added_bref = true; /* always false for substitute pattern. */
      /* -1: backref numbers starts at 1, arrays at 0. */
      for (string_ind = fre_pmatch_table->sub_match[subm_ind-1]->bo - numof_tokens;
	   string_ind < fre_pmatch_table->sub_match[subm_ind-1]->eo - numof_tokens;
	   string_ind++){
	new_pattern[np_ind++] = string[string_ind];
	++inserted_count;
      }
      ++sm_count;     /* Get next sub-match. */
      /*      ++in_array_ind;  Get next backref position. */
      ++(freg_object->bref_to_insert);
    }
    else if (freg_object->striped_pattern[is_sub][sp_ind] == '\0'
	     && sp_ind < (first_elem_pos + next_elem_pos)){
      ++sp_ind;
      continue;
    }
    else new_pattern[np_ind++] = freg_object->striped_pattern[is_sub][sp_ind++];
  }
  /* Get ready for the next call to insert_sm(). */
  freg_object->bref_to_insert = 0;
  /* Make sure we forget no characters in ->striped_pattern[]. */
  if (!is_sub)
    while(freg_object->saved_pattern[is_sub][sp_ind] != '\0')
      new_pattern[np_ind++] = freg_object->saved_pattern[is_sub][sp_ind++];
  else
    while(freg_object->striped_pattern[is_sub][sp_ind] != '\0')
      new_pattern[np_ind++] = freg_object->striped_pattern[is_sub][sp_ind++];
  new_pattern[np_ind] = '\0';
  
  if (SU_strcpy(freg_object->striped_pattern[is_sub], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }

  return inserted_count;
}


/*
 * Remove a character sequence from a NUL terminated string. 
 * No error checks are made. 
 * Self warning >>> Possible comparaison agaisnt signed vs unsigned int. <<<
 */
char* intern__fre__cut_match(char *string,
			     size_t *numof_tokens_skiped, /* Number of tokens skiped by cut_match. */
			     size_t string_size,          /* No to be confused with the lenght. */
			     size_t bo,                   /* Begining of match string index. */
			     size_t eo)                   /* Ending of match string index.   */
{
  char *new_string = NULL;
  size_t ns_ind = 0, i = 0, offset_to_start = *numof_tokens_skiped;

  if ((new_string = calloc(string_size, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    return NULL;
  }
  while (i <= string_size && string[i] != '\0'){
    if (i == (bo - offset_to_start)){
      while (i < (eo - offset_to_start)){
	++i;
	++(*numof_tokens_skiped);
      }
      continue;
    }
    new_string[ns_ind++] = string[i++];
  }
  if (string_size <= 2){
    string[0] = '\0';
  }
  else {
    if (SU_strcpy(string, new_string, string_size) == NULL){
      intern__fre__errmesg("SU_strcpy");
      if (new_string)
	free(new_string);
      return NULL;
    }
  }
  if (new_string)
    free(new_string);
  return string;

} /* intern__fre__cut_match() */
  
/* 
 * Debug hook
 * Print all values of a given fre_pattern object.
 * Vulnerable to 'use of uninitialized value', do NOT use in production code.
 */
void print_pattern_hook(fre_pattern* pat)
{
  size_t i = 0;
  int c = 0;
  if (pat == NULL){
    pthread_mutex_lock(&fre_stderr_mutex);
    fprintf(stderr, "Pattern is NULL\n");
    pthread_mutex_unlock(&fre_stderr_mutex);
    return;
  }
  pthread_mutex_lock(&fre_stderr_mutex);
  fprintf(stderr, "fre_mod_boleol %s\nfre_mod_newline %s\nfre_mod_icase %s\nfre_mod_ext %s\nfre_mod_global %s\n",
	  ((pat->fre_mod_boleol == true) ? "true" : "false"),
	  ((pat->fre_mod_newline == true) ? "true" : "false"),
	  ((pat->fre_mod_icase == true) ? "true" : "false"),
	  ((pat->fre_mod_ext == true) ? "true" : "false"),
	  ((pat->fre_mod_global == true) ? "true" : "false"));
  fprintf(stderr, "fre_p1_compiled %s\nfre_paired_delimiters %s\nDelimiter [%c]\tC_Delimiter [%c]\n",
	  ((pat->fre_p1_compiled == true) ? "true" : "false"),
	  ((pat->fre_paired_delimiters == true) ? "true" : "false"),
	  pat->delimiter, pat->c_delimiter);
  fprintf(stderr, "fre_match_op_bow: %s\nfre_match_op_eow: %s\nfre_subs_op_bow: %s\nfre_subs_op_eow: %s\n",
	  ((pat->fre_match_op_bow == true) ? "true" : "false"),
	  ((pat->fre_match_op_eow == true) ? "true" : "false"),
	  ((pat->fre_subs_op_bow == true) ? "true" : "false"),
	  ((pat->fre_subs_op_eow == true) ? "true" : "false"));
  fprintf(stderr, "Operation type: %s\nfre_match_op_bref %s\nfre_subs_op_bref %s\n",
	  ((pat->fre_op_flag == NONE) ? "NONE" :
	   (pat->fre_op_flag == MATCH) ? "MATCH" :
	   (pat->fre_op_flag == SUBSTITUTE) ? "SUBSTITUTE" :
	   (pat->fre_op_flag == TRANSLITERATE) ? "TRANSLITERATE" : "Error, invalid flag"),
	  ((pat->fre_match_op_bref == true) ? "true" : "false"),
	  ((pat->fre_subs_op_bref == true) ? "true" : "false"));
  fprintf(stderr, "backref_pos\t %-17s:", "in_pattern");
  for (i = 0; i < pat->backref_pos->in_pattern_c; i++){
    fprintf(stderr, "%d ", pat->backref_pos->in_pattern[i]);
  }
  fprintf(stderr,"\n          \t %-17s:", "sub-match number");
  for (i = 0; i < pat->backref_pos->in_pattern_c; i++){
    fprintf(stderr, "%ld ", pat->backref_pos->p_sm_number[i]);
  }
  fprintf(stderr, "\n          \t %-17s:", "in_substitute");
  for (i = 0; i < pat->backref_pos->in_substitute_c; i++)
    fprintf(stderr, "%d ", pat->backref_pos->in_substitute[i]);
  fprintf(stderr, "\n          \t %-17s:", "sub-match number");
  for (i = 0; i < pat->backref_pos->in_substitute_c; i++)
    fprintf(stderr, "%ld ", pat->backref_pos->s_sm_number[i]);
  fprintf(stderr,"\ncomp_pattern %s\n",
	  ((pat->comp_pattern != NULL) ? "Defined" : "NULL"));
  fprintf(stderr, "striped_pattern[0] %s\nstriped_pattern[1] %s\n",
	  pat->striped_pattern[0], pat->striped_pattern[1]);
  fprintf(stderr, "saved_pattern[0] %s\nsaved_pattern[1] %s\n\n",
	  pat->saved_pattern[0], pat->saved_pattern[1]);
  pthread_mutex_unlock(&fre_stderr_mutex);
  return;
}
