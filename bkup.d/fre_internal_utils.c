
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


/** Memory Allocation Functions **/

/* 
 * Allocate memory for the global headnode_table that
 * contains pointers to all pmatch-tables created since 
 * _lib_init. This to allow _lib_finit to free all used memory.
 */
fre_headnodes* intern__fre__init_head_table(void)
{
  size_t i = 0;
  fre_headnodes *headnode_table = NULL;

  if ((headnode_table = malloc(sizeof(fre_headnodes))) == NULL){
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  if ((headnode_table->table_lock = malloc(sizeof(pthread_mutex_t))) == NULL ){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  if (pthread_mutex_init(headnode_table->table_lock, NULL) != 0){
    intern__fre__errmesg("Pthread_mutex_init");
    goto errjmp;
  }

  /* Init the array of pmatch_tables. */
  if ((headnode_table->head_list = malloc(FRE_HEADNODE_TABLE_SIZE * sizeof(fre_pmatch*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < FRE_HEADNODE_TABLE_SIZE; i++){
    if ((headnode_table->head_list[i] = intern__fre__init_pmatch_table()) == NULL){
      intern__fre__errmesg("Intern__fre__init_pmatch_table");
      goto errjmp;
    }
  }

  headnode_table->sizeof_table = FRE_HEADNODE_TABLE_SIZE;
  headnode_table->head_list_tos = 0;
  
  return headnode_table; /* Success ! */

 errjmp:
  if (headnode_table != NULL){
    if (headnode_table->table_lock != NULL){
      free(headnode_table->table_lock);
      headnode_table->table_lock = NULL;
    }
    if (headnode_table->head_list != NULL){
      for (i = 0; i < FRE_HEADNODE_TABLE_SIZE; i++){
	if (headnode_table->head_list[i] != NULL){
	  intern__fre__free_pmatch_table(headnode_table->head_list[i]);
	}
      }
      free(headnode_table->head_list);
      headnode_table->head_list = NULL;
    }
  }
  return NULL;

} /* intern__fre__init_head_table() */


/* Release resources of the global headnode_table. */
void intern__fre__free_head_table(void)
{
  size_t i = 0;
  /* Return right away if we're passed a NULL argument. */
  if (fre_headnode_table == NULL)
    return;
  if (fre_headnode_table->head_list != NULL){
    for (i = 0; i < fre_headnode_table->sizeof_table; i++){
      if (fre_headnode_table->head_list[i] != NULL){
	intern__fre__free_pmatch_table(fre_headnode_table->head_list[i]);
	fre_headnode_table->head_list[i] = NULL;
      }
    }
    free(fre_headnode_table->head_list);
    fre_headnode_table->head_list = NULL;
  }
  if (fre_headnode_table->table_lock != NULL) {
    pthread_mutex_destroy(fre_headnode_table->table_lock);
    free(fre_headnode_table->table_lock);
    fre_headnode_table->table_lock = NULL;
  }
  free(fre_headnode_table);
  fre_headnode_table = NULL;

} /* intern__fre__free_head_table() */
    

/* 
 * Allocate memory for arrays holding possible 
 * back-reference(s) position(s) within the pattern(s). 
 */
fre_backref* intern__fre__init_bref_arr(void)
{
  fre_backref *to_init = NULL;

  if ((to_init = malloc(sizeof(fre_backref))) == NULL) {
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  if ((to_init->in_pattern = malloc(FRE_MAX_SUB_MATCHES * sizeof(int))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjump;
  }
  memset(to_init->in_pattern, -1, FRE_MAX_SUB_MATCHES);
  if ((to_init->p_sm_number = malloc(FRE_MAX_SUB_MATCHES * sizeof(long))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjump;
  }
  memset(to_init->p_sm_number, -1, FRE_MAX_SUB_MATCHES);
  if ((to_init->in_substitute = malloc(FRE_MAX_SUB_MATCHES * sizeof(int))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjump;
  }
  memset(to_init->in_substitute, -1, FRE_MAX_SUB_MATCHES);
  if ((to_init->s_sm_number = malloc(FRE_MAX_SUB_MATCHES * sizeof(long))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjump;
  }
  memset(to_init->s_sm_number, -1, FRE_MAX_SUB_MATCHES);
  to_init->in_pattern_c = 0;
  to_init->in_substitute_c = 0;
  return to_init;

 errjump:
  if (to_init != NULL){
    if (to_init->in_pattern != NULL){
      free(to_init->in_pattern);
      to_init->in_pattern = NULL;
    }
    if (to_init->p_sm_number != NULL){
      free(to_init->p_sm_number);
      to_init->p_sm_number = NULL;
    }
    if (to_init->in_substitute != NULL){
      free(to_init->in_substitute);
      to_init->in_substitute = NULL;
    }
    if (to_init->s_sm_number != NULL){
      free(to_init->s_sm_number);
      to_init->s_sm_number = NULL;
    }
    free(to_init);
    to_init = NULL;
  }
  return NULL;
} /* intern__fre__init_bref_arr() */


/* 
 * Release resources of a fre_backref object,
 * previously initialized via intern__fre__init_bref_arr().
 */
void intern__fre__free_bref_arr(fre_backref *to_free)
{
  /* Return right away if we're being passed a NULL argument. */
  if (!to_free){
    return;
  }
  if (to_free->in_pattern != NULL){
    free(to_free->in_pattern);
    to_free->in_pattern = NULL;
  }
  if (to_free->p_sm_number != NULL){
    free(to_free->p_sm_number);
    to_free->p_sm_number = NULL;
  }
  if (to_free->in_substitute != NULL){
    free(to_free->in_substitute);
    to_free->in_substitute = NULL;
  }
  if (to_free->s_sm_number != NULL){
    free(to_free->s_sm_number);
    to_free->s_sm_number = NULL;
  }
  free(to_free);
  /*  to_free = NULL;*/
  return;
} /* intern__fre__free_bref_arr() */


  
/* 
 * Allocate memory to a fre_smatch structure, 
 * used to register [sub]matches positions inside a matching string.
 * There's no deallocation function, free is being called by
 * intern__fre__free_pmatch_table().
 */
fre_smatch* intern__fre__init_smatch(void)
{
  fre_smatch *to_init = NULL;
  if ((to_init = malloc(sizeof(fre_smatch))) == NULL){
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  to_init->bo = -1;
  to_init->eo = -1;
  return to_init;

} /* intern__fre__init_smatch() */


/* 
 * Allocate memory for a single pmatch_table,
 * a per-thread table of [sub]matches positions kept 
 * between each invocation of fre_bind.
 */
fre_pmatch* intern__fre__init_pmatch_table(void)
{
  size_t i = 0;
  fre_pmatch *to_init = NULL;

  if ((to_init = malloc(sizeof(fre_pmatch))) == NULL){
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  if ((to_init->ls_pattern = calloc(FRE_MAX_PATTERN_LENGHT, sizeof(char))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjmp;
  }
  if ((to_init->whole_match = malloc(FRE_MAX_MATCHES * sizeof(fre_smatch*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < FRE_MAX_MATCHES; i++){
    if ((to_init->whole_match[i] = intern__fre__init_smatch()) == NULL){
      intern__fre__errmesg("Intern__fre__init_smatch");
      goto errjmp;
    }
  }
  if ((to_init->sub_match = malloc(FRE_MAX_SUB_MATCHES * sizeof(fre_smatch*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < FRE_MAX_SUB_MATCHES; i++){
    if ((to_init->sub_match[i] = intern__fre__init_smatch()) == NULL){
      intern__fre__errmesg("Intern__fre__init_smatch");
      goto errjmp;
    }
  }
  to_init->subm_per_match = 0;
  to_init->lastop_retval = 0;
  to_init->fre_saved_object = false;
  to_init->wm_ind = 0;
  to_init->sm_ind = 0;
  to_init->wm_size = FRE_MAX_MATCHES;
  to_init->sm_size = FRE_MAX_SUB_MATCHES;
  to_init->bref_to_insert = 0;

  return to_init; /* Success! */

 errjmp:
  if (to_init != NULL){
    if (to_init->ls_pattern){
      free(to_init->ls_pattern);
      to_init->ls_pattern = NULL;
    }
    if (to_init->whole_match != NULL){
      for (i = 0; i < FRE_MAX_MATCHES; i++){
	if (to_init->whole_match[i] != NULL){
	  free(to_init->whole_match[i]);
	  to_init->whole_match[i] = NULL;
	}
      }      
      free(to_init->whole_match);
      to_init->whole_match = NULL;
    }
    if (to_init->sub_match != NULL){
      for (i = 0; i < FRE_MAX_SUB_MATCHES; i++){
	if (to_init->sub_match[i] != NULL){
	  free(to_init->sub_match[i]);
	  to_init->sub_match[i] = NULL;
	}
      }
      free(to_init->sub_match);
      to_init->sub_match = NULL;
    }
    to_init->ls_object = NULL;
    free(to_init);
    to_init = NULL;
  }

  return NULL; /* Failure. */
} /* intern__fre__init_pmatch_table() */


/* Release memory of a single pmatch_table. */
void intern__fre__free_pmatch_table(fre_pmatch *to_free)
{
  size_t i = 0;
  /*  If we're being passed a NULL argument, return now. */
  if (to_free != NULL){
    if (to_free->sub_match != NULL){
      for (i = 0; i < to_free->sm_size; i++){
	if (to_free->sub_match[i] != NULL){
	  free(to_free->sub_match[i]);
	  to_free->sub_match[i] = NULL;
	}
      }
      free(to_free->sub_match);
      to_free->sub_match = NULL;
    }
    if (to_free->whole_match != NULL){
      for (i = 0; i < to_free->wm_size; i++){
	if (to_free->whole_match[i] != NULL){
	  free(to_free->whole_match[i]);
	  to_free->whole_match[i] = NULL;
	}
      }
      free(to_free->whole_match);
      to_free->whole_match = NULL;
    }
    if (to_free->ls_pattern != NULL){
      free(to_free->ls_pattern);
      to_free->ls_pattern = NULL;
    }
    to_free->ls_object = NULL;

    free(to_free);
  }

  return;
  
} /* intern__fre__free_pmatch_table() */



/* 
 * Extend the Ptable's whole_match list when it's short on free space.
 * listnum == 0: whole_match list; listnum == 1: sub_match list;
 */
int intern__fre__extend_ptable_list(int listnum){
  fre_smatch **temp = NULL;
  size_t i = ((listnum) ? fre_pmatch_table->sm_ind : fre_pmatch_table->wm_ind);
  size_t newsize = ((listnum) ? fre_pmatch_table->sm_size*2 : fre_pmatch_table->wm_size*2);
  if (listnum){
    if ((temp = realloc(fre_pmatch_table->sub_match, newsize * sizeof(fre_smatch*))) == NULL){
      intern__fre__errmesg("Realloc");
      return FRE_ERROR;
    }
  }
  else{
    if ((temp = realloc(fre_pmatch_table->whole_match, newsize * sizeof(fre_smatch*))) == NULL){
      intern__fre__errmesg("Realloc");
      return FRE_ERROR;
    }
  }
  while (i < newsize)
    if ((temp[i] = intern__fre__init_smatch()) == NULL){
      intern__fre__errmesg("Intern__fre__init_smatch");
      goto errjmp;
    }
  if (listnum) {
    fre_pmatch_table->sub_match = temp;
    fre_pmatch_table->sm_size = newsize;
  }
  else {
    fre_pmatch_table->whole_match = temp;
    fre_pmatch_table->wm_size = newsize;
  }
  temp = NULL;
  return FRE_OP_SUCCESSFUL;

 errjmp:
  if (temp)
    for (i = 0; i < newsize; i++)
      if(temp[i]){
	free(temp[i]);
	temp[i] = NULL;
      }
  return FRE_ERROR;

} /* intern__fre__extend_ptable_list() */



/* 
 * Initialize a fre_pattern, a structure containing about all
 * the information needed to complete the pattern's requested operation.
 */
fre_pattern* intern__fre__init_pattern(void)
{
  size_t i = 0;
  fre_pattern *freg_object = NULL;

  if ((freg_object = malloc(sizeof(fre_pattern))) == NULL){
    intern__fre__errmesg("Malloc");
    return NULL;
  }

  if ((freg_object->backref_pos = intern__fre__init_bref_arr()) == NULL){
    intern__fre__errmesg("Intern__fre__init_bref_arr");
    goto errjmp;
  }
  if ((freg_object->comp_pattern = malloc(sizeof(regex_t))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  /* 
   * We need exactly 2 strings, 
   * 1 for the "matching pattern",
   * 1 for the "substitute pattern".
   */
  if ((freg_object->striped_pattern = malloc(2 * sizeof(char*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < 2; i++){
    if ((freg_object->striped_pattern[i] = calloc(FRE_MAX_PATTERN_LENGHT, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      goto errjmp;
    }
  }
  if ((freg_object->saved_pattern = malloc(2 * sizeof(char*))) == NULL){
    intern__fre__errmesg("Malloc");
    goto errjmp;
  }
  for (i = 0; i < 2; i++){
    if ((freg_object->saved_pattern[i] = calloc(FRE_MAX_PATTERN_LENGHT, sizeof(char))) == NULL){
      intern__fre__errmesg("Calloc");
      goto errjmp;
    }
  }
  
  /* Initialize all other fields. */
  freg_object->fre_mod_boleol = false;
  freg_object->fre_mod_newline = false;
  freg_object->fre_mod_icase = false;
  freg_object->fre_mod_ext = false;
  freg_object->fre_mod_global = false;
  freg_object->fre_mod_sub_is_regex = false;
  freg_object->fre_p1_compiled = false;
  freg_object->fre_not_boundary = false;
  freg_object->fre_match_op_bow = false;
  freg_object->fre_match_op_eow = false;
  freg_object->fre_subs_op_bow = false;
  freg_object->fre_subs_op_eow = false;
  freg_object->fre_paired_delimiters = false;
  freg_object->delimiter = '\0';
  freg_object->c_delimiter = '\0';
  freg_object->fre_op_flag = NONE;
  freg_object->fre_match_op_bref = false;
  freg_object->fre_subs_op_bref = false;
  
  /* All set. */
  return freg_object;

 errjmp:
  if (freg_object != NULL){
    if (freg_object->backref_pos != NULL){
      intern__fre__free_bref_arr(freg_object->backref_pos);
      freg_object->backref_pos = NULL;
    }
    if (freg_object->comp_pattern != NULL){
      free(freg_object->comp_pattern);
      freg_object->comp_pattern = NULL;
    }
    if (freg_object->striped_pattern != NULL){
      for (i = 0; i < 2; i++){
	if (freg_object->striped_pattern[i] != NULL){
	  free(freg_object->striped_pattern[i]);
	  freg_object->striped_pattern[i] = NULL;
	}
      }
      free(freg_object->striped_pattern);
      freg_object->striped_pattern = NULL;
    }
    if (freg_object->saved_pattern != NULL){
      for (i = 0; i < 2; i++){
	if (freg_object->saved_pattern[i] != NULL){
	  free(freg_object->saved_pattern[i]);
	  freg_object->saved_pattern[i] = NULL;
	}
      }
      free(freg_object->saved_pattern);
      freg_object->saved_pattern = NULL;
    }

    free(freg_object);
    freg_object = NULL;
  }
  return NULL;
} /* intern__fre__init_pattern() */


/* Release resources used by a fre_pattern object. */
void intern__fre__free_pattern(fre_pattern *freg_object)
{
  size_t i = 0;
  /* Return right away if we're passed a NULL object. */
  if (freg_object == NULL)
    return;
  if (freg_object->backref_pos != NULL){
    intern__fre__free_bref_arr(freg_object->backref_pos);
    freg_object->backref_pos = NULL;
  }
  /* Check if fre_p1_compiled is true, if yes regfree the pattern first. */
  if (freg_object->comp_pattern != NULL){
    if (freg_object->fre_p1_compiled == true){
      regfree(freg_object->comp_pattern);
    }
    free(freg_object->comp_pattern);
    freg_object->comp_pattern = NULL;
  }
  if (freg_object->striped_pattern != NULL){
    for (i = 0; i < 2; i++){
      if (freg_object->striped_pattern[i] != NULL){
	free(freg_object->striped_pattern[i]);
	freg_object->striped_pattern[i] = NULL;
      }
    }
    free(freg_object->striped_pattern);
    freg_object->striped_pattern = NULL;
  }
  if (freg_object->saved_pattern != NULL){
    for (i = 0; i < 2; i++){
      if (freg_object->saved_pattern[i] != NULL){
	free(freg_object->saved_pattern[i]);
	freg_object->saved_pattern[i] = NULL;
      }
    }
    free(freg_object->saved_pattern);
    freg_object->saved_pattern = NULL;
  }

  free(freg_object);

  return;
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



/** Regex Parser Utility Functions. **/





/* 
 * Registers a backreference's position within the pattern it's been found
 * and also registers its real backreference number (the one found after the '\' or '$').
 */
#define FRE_HANDLE_BREF(pattern, token_ind, sub_match_ind, fre_is_sub, freg_object)do {	\
    char refnum_string[FRE_MAX_PATTERN_LENGHT];				\
    long refnum = 0;							\
    size_t i = 0;							\
    if (!fre_is_sub) {							\
      freg_object->backref_pos->in_pattern[freg_object->backref_pos->in_pattern_c] = sub_match_ind; \
      freg_object->fre_match_op_bref = true;				\
    }									\
    else {								\
      freg_object->backref_pos->in_substitute[freg_object->backref_pos->in_substitute_c] = sub_match_ind; \
      freg_object->fre_subs_op_bref = true;				\
    }									\
    memset(refnum_string, 0, FRE_MAX_PATTERN_LENGHT);			\
    ++(*token_ind);							\
    do {								\
      refnum_string[i++] = pattern[(*token_ind)++];			\
    } while (isdigit(*token_ind));					\
    refnum = atol(refnum_string);					\
    if (!fre_is_sub){							\
      freg_object->backref_pos->p_sm_number[freg_object->backref_pos->in_pattern_c++] = refnum;	\
    }									\
    else{								\
      freg_object->backref_pos->s_sm_number[freg_object->backref_pos->in_substitute_c++] = refnum; \
    }									\
  } while(0); 


/*
 * Checks each characters inside the \Q..\E|\0 to see if it's a metacharacter.
 * When encountering a POSIX metacharacter, checks to see if the new pattern has
 * enough free memory to hold an extra backslash and push a backslash before pushing the
 * actual metacharacter to the pattern's stack.

 * Takes the fre_pattern carried throughout the library, the token of the backslash 
 * begining the \Q... sequence, the pattern being built is qfreg_object->striped_pattern[0]
 * (freg_object->striped_pattern[1] isn't allowed to reach that far in perl_to_posix)
 * the new pattern's top of stack and lenght.

 * Make careful use of this MACRO since it may returns an integer.
 */
#define FRE_QUOTE_TOKENS(qfreg_object, qpattern, qtoken_ind, qnew_pat_tos, qnew_pat_len) do { \
    size_t new_pat_len = qnew_pat_len;					\
    (*qtoken_ind) += 2; /* To be at the first token of the sequence */	\
    while(qpattern[*qtoken_ind] != '\0'){				\
      if (qpattern[*qtoken_ind] == '\\'){				\
	if (qpattern[(*qtoken_ind)+1] == 'E') {				\
	  (*qtoken_ind) += 2;						\
	  break;							\
	}								\
      }									\
      if ((qpattern[*qtoken_ind] == qfreg_object->delimiter && qfreg_object->fre_paired_delimiters == false) \
	  || (qpattern[*qtoken_ind] == qfreg_object->c_delimiter && qfreg_object->fre_paired_delimiters == true)) break; \
      switch(qpattern[*qtoken_ind]) {					\
      case '.' : case '[' : case '\\':					\
      case '(' : case ')' : case '*' :					\
      case '+' : case '?' : case '{' :					\
      case '|' : case '^' : case '$' :					\
	if ((new_pat_len += 1) >= FRE_MAX_PATTERN_LENGHT){		\
	  errno = 0; intern__fre__errmesg("Overflow replacing Perl-like \\Q...\\E escape sequence"); \
	  errno = EOVERFLOW;						\
	  return FRE_ERROR;						\
	}								\
	FRE_PUSH('\\', qfreg_object->striped_pattern[0], qnew_pat_tos);	\
	break;								\
      }									\
      FRE_PUSH(qpattern[*qtoken_ind], qfreg_object->striped_pattern[0], qnew_pat_tos); \
      (*qtoken_ind) += 1;						\
    }									\
  } while (0);

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
 * Replaces a word boundary token sequence if we can, else raise
 * the appropriate fre_pattern flag.
 
 * Takes the fre_pattern, the ->striped_pattern's top of stack, the new pattern _p_t_p() is builing, its top of stack
 * and lenght, a 0 or 1 value indicating whether this is the matching or substitute pattern and a 0 or 1
 * value indicating whether it's a word boundary sequence: '\<' '\>' '\b' or a not a word boundary sequence '\B'.

 * This MACRO might return an integer, careful about using it elsewhere than _perl_to_posix().
 */
#define FRE_REGISTER_BOUNDARY(freg_objet, spa_tos, new_pat, new_pat_tos, new_pat_len, is_sub, is_word) do{ \
    size_t i = 0;							\
    if (spa_tos == 0){							\
      if (!is_sub) freg_object->fre_match_op_bow = true;		\
      else freg_object->fre_subs_op_bow = true;				\
    } else if (spa_tos == (strlen(freg_object->striped_pattern[is_sub])-2)){ \
      if (!is_sub) freg_object->fre_match_op_eow = true;		\
      else freg_object->fre_subs_op_eow = true;				\
    }									\
    else {								\
      if (is_word){							\
	if ((*new_pat_len += strlen(FRE_POSIX_NON_WORD_CHAR)+1) >= FRE_MAX_PATTERN_LENGHT){ \
	  errno = 0; intern__fre__errmesg("Overflow replacing a word boundary sequence"); \
	  errno = EOVERFLOW; return FRE_ERROR;				\
	}								\
	while(FRE_POSIX_NON_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_WORD_CHAR[i++], new_pat, new_pat_tos); \
      }	else {								\
	if ((*new_pat_len += strlen(FRE_POSIX_WORD_CHAR)+1) >= FRE_MAX_PATTERN_LENGHT){ \
	  errno = 0; intern__fre__errmesg("Overflow replacing a word boundary sequence"); \
	  errno = EOVERFLOW; return FRE_ERROR;				\
	}								\
	while (FRE_POSIX_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_WORD_CHAR[i++], new_pat, new_pat_tos); \
      }									\
    }									\
  } while (0);

/*
 * This MACRO is used to verify each escape sequence found in the user's given pattern,
 * and replace any sequences not supported by the POSIX standard. 
 * Takes the parsed pattern, the token index of the backslash leading the escape sequence,
 * the new pattern made by _perl_to_posix, the new pattern's top of stack, the new pattern's
 * current lenght and the fre_pattern object used throughout the library.

 * This MACRO returns an integer, careful about using it elsewhere than _perl_to_posix()!
 */
#define FRE_CERTIFY_ESC_SEQ(is_sub, token_ind, new_pat, new_pat_tos, new_pat_len, freg_object) do { \
    size_t i = 0;							\
    switch(freg_object->striped_pattern[is_sub][*token_ind + 1]){	\
    case 'd':								\
      if ((*new_pat_len += strlen(FRE_POSIX_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Pattern lenght exceed maximum converting Perl-like digit range '\\d'"); \
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while (FRE_POSIX_DIGIT_RANGE[i] != '\0') FRE_PUSH(FRE_POSIX_DIGIT_RANGE[i++], new_pat, new_pat_tos); \
      /* Adjust the token index. */					\
      *token_ind += 2;							\
      break;								\
    case 'D':								\
      if ((*new_pat_len += strlen(FRE_POSIX_NON_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Pattern lenght exceed maximum converting Perl-like not digit range '\\D'"); \
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while(FRE_POSIX_NON_DIGIT_RANGE[i] != '\0') FRE_PUSH(FRE_POSIX_NON_DIGIT_RANGE[i++], new_pat, new_pat_tos); \
      /* Adjust the token index. */					\
      *token_ind += 2;							\
      break;								\
    case 'w':								\
      if ((*new_pat_len += strlen(FRE_POSIX_WORD_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow convering Perl-like '\\w' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while (FRE_POSIX_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_WORD_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;							\
      break;								\
      /* More test will go here. */					\
    case 'W':								\
      if ((*new_pat_len += strlen(FRE_POSIX_NON_WORD_CHAR)) >= FRE_MAX_PATTERN_LENGHT){	\
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\W' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while (FRE_POSIX_NON_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_WORD_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;							\
      break;								\
    case 's':								\
      if ((*new_pat_len += strlen(FRE_POSIX_SPACE_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\s' into a POSIX construct.");	\
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while (FRE_POSIX_SPACE_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_SPACE_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;							\
      break;								\
    case 'S':								\
      if ((*new_pat_len += strlen(FRE_POSIX_NON_SPACE_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\S' into a POSIX construct.");	\
	errno = EOVERFLOW; return FRE_ERROR;				\
      }									\
      while (FRE_POSIX_NON_SPACE_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_SPACE_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;							\
      break;								\
    case 'b': /* Fall throught */					\
    case '<': /* Fall throught */					\
    case '>':								\
      FRE_REGISTER_BOUNDARY(freg_object, *token_ind, new_pat,		\
			    new_pat_tos, new_pat_len, is_sub, 1);	\
      *token_ind += 2;							\
      break;								\
    case 'B':								\
      FRE_REGISTER_BOUNDARY(freg_object, *token_ind, new_pat,		\
			    new_pat_tos, new_pat_len, is_sub, 0);	\
      freg_object->fre_not_boundary = true;				\
      *token_ind += 2;							\
      break;								\
    default:								\
      /* Assume a supported sequence, push the tokens on the pattern stack. */ \
      FRE_PUSH(freg_object->striped_pattern[is_sub][(*token_ind)++], new_pat, new_pat_tos); \
      FRE_PUSH(freg_object->striped_pattern[is_sub][(*token_ind)++], new_pat, new_pat_tos); \
      /*(*token_ind) += 2;*/						\
      break;								\
    }									\
  }while (0);

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
    if (fre_do_not_push == false)
      FRE_PUSH(FRE_TOKEN, new_pattern, &new_pattern_tos);
    
    ++token_ind; /* Get next token. */
  }
  if (SU_strcpy(freg_object->striped_pattern[is_sub], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  return FRE_OP_SUCCESSFUL;
  
} /* intern__fre__perl_to_posix() */



/* Insert all sub-matches into the given pattern. */
int intern__fre__insert_sm(fre_pattern *freg_object,      /* The object used throughout the library. */
			   char *string,                  /* The string to match. */
			   int numof_tokens,              /* Number of tokens skiped by a global operation. */
			   size_t is_sub)
{
  int string_ind = 0, sp_ind = 0, np_ind = 0, in_array_ind = 0;
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
  /*
   * Loop over each characters of ->striped_pattern[is_sub] till we reach
   * the ->bref_to_insert backreference's position 
   * then insert the sub-match corresponding to the position 
   * we're at into the new_pattern. Finaly, increment the ptable's ->bref_to_insert field
   * to get ready for the next call.
   */
  while (sp_ind < FRE_MAX_PATTERN_LENGHT) {
    long subm_ind = ((!is_sub)? freg_object->backref_pos->p_sm_number[sm_count]
		     : freg_object->backref_pos->s_sm_number[sm_count]);
    if (in_array[in_array_ind] == -1) break;
    if (in_array_ind != 0)
      next_elem_pos = in_array[in_array_ind];
    if (sp_ind == first_elem_pos + next_elem_pos){
      /* -1: backref numbers starts at 1, arrays at 0. */
      for (string_ind = fre_pmatch_table->sub_match[subm_ind-1]->bo - numof_tokens;
	   string_ind < fre_pmatch_table->sub_match[subm_ind-1]->eo - numof_tokens;
	   string_ind++){
	new_pattern[np_ind++] = string[string_ind];
	++inserted_count;
      }
      ++sm_count;
      ++in_array_ind; /* Get next backref position. */
    }
    else if (freg_object->striped_pattern[is_sub][sp_ind] == '\0'
	     && sp_ind < (first_elem_pos + next_elem_pos)){
      ++sp_ind;
      continue;
    }
    else new_pattern[np_ind++] = freg_object->striped_pattern[is_sub][sp_ind++];
  }

  /* Make sure we forget no characters in ->striped_pattern[]. */
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

