/*
 *
 *  Libfre  -  Main internal functions.
 *  Version:   0.400
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
  
  /* 
   * Convert a stripped Perl-like pattern, that's not a transliteration opeartion,
   * into a POSIX ERE conformant pattern. 
   */
  if (freg_object->fre_op_flag != TRANSLITERATE){
    if (intern__fre__perl_to_posix(freg_object, 0) != FRE_OP_SUCCESSFUL){
      intern__fre__errmesg("Intern__fre__perl_to_posix");
      goto errjmp;
    }
    if (SU_strcpy(freg_object->saved_pattern[0], freg_object->striped_pattern[0], FRE_MAX_PATTERN_LENGHT) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }
    if (freg_object->fre_op_flag == SUBSTITUTE) {
      if (intern__fre__perl_to_posix(freg_object, 1) != FRE_OP_SUCCESSFUL){
	intern__fre__errmesg("Intern__fre__perl_to_posix");
	goto errjmp;
      }
      if (SU_strcpy(freg_object->saved_pattern[1], freg_object->striped_pattern[1], FRE_MAX_PATTERN_LENGHT) == NULL){
	intern__fre__errmesg("SU_strcpy");
	goto errjmp;
      }
    }
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



/* MODIFY errjmp to handle failure on fre_smatch reallocations. */
#define WM_IND fre_pmatch_table->wm_ind
#define SM_IND fre_pmatch_table->sm_ind

int intern__fre__match_op(char *string,                  /* The string to bind the pattern against. */
			  fre_pattern *freg_object,      /* The information gathered by the _plp_parser(). */
			  size_t *offset_to_start)       /* To keep string positions alignment in global operations. */
{
  bool bref_flag_wastrue = freg_object->fre_match_op_bref;
  int reg_retval = 0;
  char *string_copy = NULL;
  /*  size_t offset_to_start = 0;                             To keep string position alignment in global operations. */
  size_t string_len = 0;
  size_t i = 0, reg_i = 0;
  /*  size_t *sm_ind = &fre_pmatch_table->sm_ind;
      size_t *wm_ind = &fre_pmatch_table->wm_ind;*/
  size_t new_size = 0;
  regmatch_t regmatch_arr[FRE_MAX_SUB_MATCHES];
  fre_smatch **temp = NULL;


  if (!string || !freg_object){
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
    if (fre_pmatch_table->lastop_retval != FRE_OP_SUCCESSFUL)
      fre_pmatch_table->lastop_retval = FRE_OP_UNSUCCESSFUL;
    goto skip_match; /* Above the return statement. */
  }

  /* Match! */
  fre_pmatch_table->lastop_retval = FRE_OP_SUCCESSFUL;
  if (regmatch_arr[reg_i].rm_so != -1){
    fre_pmatch_table->whole_match[WM_IND]->bo = regmatch_arr[reg_i].rm_so + *offset_to_start;
    fre_pmatch_table->whole_match[WM_IND]->eo = regmatch_arr[reg_i].rm_eo + *offset_to_start;
  }
  ++reg_i;
  while(regmatch_arr[reg_i].rm_so != -1){
    fre_pmatch_table->sub_match[SM_IND]->bo = regmatch_arr[reg_i].rm_so + *offset_to_start;
    fre_pmatch_table->sub_match[SM_IND]->eo = regmatch_arr[reg_i].rm_eo + *offset_to_start;
    if (++(SM_IND) == fre_pmatch_table->sm_size){
      new_size = fre_pmatch_table->sm_size * 2;
      if ((temp = realloc(fre_pmatch_table->sub_match, new_size * sizeof(fre_smatch*))) == NULL){
	intern__fre__errmesg("Realloc");
	goto errjmp;
      }
      for (i = SM_IND; i < new_size; i++){
	if ((temp[i] = intern__fre__init_smatch()) == NULL){
	  intern__fre__errmesg("Intern__fre__init_smatch");
	  goto errjmp;
	}
      }
      fre_pmatch_table->sm_size = new_size;
      fre_pmatch_table->sub_match = temp;
      temp = NULL;
    }
    ++reg_i;
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
			       fre_pmatch_table->whole_match[WM_IND]->bo,
			       fre_pmatch_table->whole_match[WM_IND]->eo) == NULL){
      intern__fre__errmesg("Intern__fre__cut_match");
      goto errjmp;
    }
    /* 
     * Check if the whole_match index matches the sizeof whole_match,
     * if yes extend the list.
     */
    if (++(WM_IND) == fre_pmatch_table->wm_size) {
      new_size = fre_pmatch_table->wm_size * 2;
      if ((temp = realloc(fre_pmatch_table->whole_match, new_size * sizeof(fre_smatch*))) == NULL){
	intern__fre__errmesg("Realloc");
	goto errjmp;
      }
      for (i = WM_IND; i < new_size; i++){
	if ((temp[i] = intern__fre__init_smatch()) == NULL){
	  intern__fre__errmesg("intern__fre__init_smatch");
	  goto errjmp;
	}
      }
      fre_pmatch_table->wm_size = new_size;
      fre_pmatch_table->whole_match = temp;
      temp = NULL;
    }

    if (string_copy[0] != '\0'){
      if (intern__fre__match_op(string_copy, freg_object, offset_to_start) == FRE_ERROR){
	intern__fre__errmesg("Intern__fre__match_op");
	goto errjmp;
      }
    }
  }
 skip_match:
  if (string_copy)
    free(string_copy);
  string_copy = NULL;
  /* Restore the pmatch_table's headnode. */
    
  return fre_pmatch_table->lastop_retval;

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
  char *string_copy = NULL;
  size_t new_string_lenght = 0;
  size_t numof_tokens_skiped = 0;
  size_t ns_ind = 0, sp_ind = 0;

  
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

  if ((new_string = calloc(string_size, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    return FRE_ERROR;
  }
  if ((string_copy = calloc(string_size, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjmp;
  }
  if (SU_strcpy(string_copy, string, string_size) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjmp;
  }
  WM_IND = 0; /* _match_op incremented it. */
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
      if (i == fre_pmatch_table->whole_match[WM_IND]->bo) {
	if (intern__fre__cut_match(string_copy, &numof_tokens_skiped, string_size,
				   (fre_pmatch_table->whole_match[WM_IND]->bo - numof_tokens_skiped),
				   (fre_pmatch_table->whole_match[WM_IND]->eo - numof_tokens_skiped)) == NULL){
	  intern__fre__errmesg("Intern__fre__cut_match");
	  goto errjmp;
	}
	/* Make sure there's enough room in "string" to hold the substitution. */
	if ((new_string_lenght += (fre_pmatch_table->whole_match[WM_IND]->eo - fre_pmatch_table->whole_match[WM_IND]->bo))
	    >= (string_size - 1)) {
	  errno = EOVERFLOW;
	  intern__fre__errmesg("Intern__fre__substitute_op");
	  goto errjmp;
	}

	/* Substitute now. */
	sp_ind = 0;
	while(freg_object->striped_pattern[1][sp_ind] != '\0')
	  new_string[ns_ind++] = freg_object->striped_pattern[1][sp_ind++];
	while (i < fre_pmatch_table->whole_match[WM_IND]->eo) i++;
	

	/* Get the next match if there's one. */
	++WM_IND;
	if (fre_pmatch_table->whole_match[WM_IND] != NULL
	    && fre_pmatch_table->whole_match[WM_IND]->bo != -1){
	  /* Restore the substitute pattern to a clean state. */
	  if (freg_object->fre_subs_op_bref == true){
	    if (freg_object->saved_pattern[1][0] == '\0'){
	      freg_object->striped_pattern[1][0] = '\0';
	    }
	    else {
	      if (SU_strcpy(freg_object->striped_pattern[1], freg_object->saved_pattern[1], FRE_MAX_PATTERN_LENGHT) == NULL){
		intern__fre__errmesg("SU_strcpy");
		goto errjmp;
	      }
	    }
	    /* Replace the new backrefs if any. */
	    if (intern__fre__insert_sm(freg_object, string, 1) == NULL){
	      intern__fre__errmesg("Intern__fre__insert_sm");
	      goto errjmp;
	    }
	  }
	}
	continue;
      }
      new_string[ns_ind++] = string[i];
      i++;
    }
    if (SU_strcpy(string, new_string, string_size) == NULL){
      intern__fre__errmesg("SU_strcpy");
      goto errjmp;
    }
    free(new_string);
    new_string = NULL;
    free(string_copy);
    string_copy = NULL;
    return (fre_pmatch_table->lastop_retval = FRE_OP_SUCCESSFUL);  
  }
  /* No match. */
  else {
    if (new_string){
      free(new_string);
      new_string = NULL;
    }
    if (string_copy){
      free(string_copy);
      string_copy = NULL;
    }
    return (fre_pmatch_table->lastop_retval = FRE_OP_UNSUCCESSFUL);
  }

 errjmp:
  if (new_string){
    free(new_string);
    new_string = NULL;
  }
  if (string_copy){
    free(string_copy);
    string_copy = NULL;
  }
  return FRE_ERROR;

} /* intern__fre__substitute_op() */


/* MUST ONLY REMOVE THE LEADING [ AND TRAILING ] !!!!! */
#define FRE_INSERT_DIGIT_RANGE(array, token_ind) do {			\
    size_t i = 1; /* The zeroth char of FRE_POSIX_DIGIT_RANGE is '[' */	\
    while (FRE_POSIX_DIGIT_RANGE[i] != ']') {				\
      array[(*token_ind)++] = FRE_POSIX_DIGIT_RANGE[i++];		\
    }									\
  } while (0);

/* 
 * Verify and insert a dash separated range. 
 * A dash separated range may be between any two printable characters,
 * so long as they appear in increasing order. It depends greatly on your locale.
 * Starting at *token_ind, insert the appropriate range in array.

 * ASCII only!!!
*/
void FRE_INSERT_DASH_RANGE(char *array, size_t *token_ind, int low, int high)
{
  do {
    int temp = 0;
    int current = low;
    if (low >= high){
      temp = low; low = high; high = temp;
    }
    while (current <= high)
      array[(*token_ind)++] = current++;
  } while (0);
}

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
	FRE_INSERT_DIGIT_RANGE(new_striped_p[sp_ind], &j);
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
    errno = 0;
    intern__fre__errmesg("Syntax error: Non-matching number of tokens in transliteration operation pattern.");
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
  



void print_ptable_hook(void)
{
  size_t i = 0, n = 0;
  pthread_mutex_lock(&fre_stderr_mutex);
  fprintf(stderr, "fre_saved_object: %s\n", ((fre_pmatch_table->fre_saved_object == true) ? "true" : "false"));
  fprintf(stderr, "lastop_retval: %d\n", fre_pmatch_table->lastop_retval);
  fprintf(stderr, "ls_pattern: %s\n", ((fre_pmatch_table->ls_pattern[0] != '\0') ? fre_pmatch_table->ls_pattern : "NULL"));
  fprintf(stderr, "ls_object: %p\nWhole_match positions:\n", ((void*)fre_pmatch_table->ls_object));
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

  fprintf(stderr,"\n\n");
  pthread_mutex_unlock(&fre_stderr_mutex);
}
