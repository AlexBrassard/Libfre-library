
/*
 *
 *
 *  Libfre  -  Internal utilities.
 *  Version:   0.010
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>   /* SIZE_MAX */
#include <errno.h>

#include "fre_internal.h"

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
 * contains pointers to all TSD linked-lists created since 
 * _lib_init. This to allow _lib_finit to free all used memory.
 */
fre_headnodes* intern__fre__init_head_table(void)
{
  size_t i = 0,c = 0 ;
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
  /* For each element of the array, init a pmatch_table linked-list. */
  for (i = 0; i < FRE_HEADNODE_TABLE_SIZE; i++){
    if ((headnode_table->head_list[i] = intern__fre__init_ptable(FRE_MAX_SUB_MATCHES)) == NULL){
      intern__fre__errmesg("Intern__fre__init_ptable");
      goto errjmp;
    }
  }

  headnode_table->sizeof_table = FRE_HEADNODE_TABLE_SIZE;
  /* -1: Must be set to the first free available node. */
  headnode_table->head_list_tos = (FRE_HEADNODE_TABLE_SIZE - 1);
  
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
	  intern__fre__free_ptable(headnode_table->head_list[i]);
	}
      }
      free(headnode_table->head_list);
      headnode_table->head_list = NULL;
    }
  }
  return NULL;

} /* intern__fre__init_head_table() */


/* Release resources of the global headnode_table. */
void intern__fre__free_head_table(void)//fre_headnodes *headnode_table)
{
  size_t i = 0;
  /* Return right away if we're passed a NULL argument. */
  if (fre_headnode_table == NULL)
    return;
  if (fre_headnode_table->head_list != NULL){
    for (i = 0; i < fre_headnode_table->sizeof_table; i++){
      if (fre_headnode_table->head_list[i] != NULL){
	intern__fre__free_ptable(fre_headnode_table->head_list[i]);
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
  if (to_free == NULL){
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
  to_free = NULL;
  return;
} /* intern__fre__free_bref_arr() */


/* 
 * Allocate memory to a fre_smatch structure, 
 * used to register sub-match(es) position(s) inside a matching string.
 * There's no deallocation function, free is being called by
 * intern__fre__free_pmatch_node().
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
 * Allocate memory to a single node of a pattern's
 * pmatch_table, a "per pattern" linked-list returned by
 * each call to fre_bind(). This table holds match and sub-match(es)'
 * position(s), or -1(s) if pattern didn't match.
 * Libfre user(s) must free this table themself.
 * Each time a user calls on fre_bind(), a fre_pmatch linked-list is being returned.
 */
fre_pmatch* intern__fre__init_pmatch_node(void)
{
  size_t i = 0;
  fre_pmatch *to_init = NULL;

  if ((to_init = malloc(sizeof(fre_pmatch))) == NULL){
    intern__fre__errmesg("Malloc");
    return NULL;
  }
  if ((to_init->whole_match = intern__fre__init_smatch()) == NULL){
    intern__fre__errmesg("Intern__fre__init_smatch");
    goto errjmp;
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
  to_init->fre_mod_global = false;
  to_init->next_match = NULL;
  to_init->headnode = NULL;
  to_init->last_op_ret_val = 0;

  return to_init; /* Success! */

 errjmp:
  if (to_init != NULL){
    if (to_init->whole_match != NULL){
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
    to_init->next_match = NULL;
    to_init->headnode = NULL;
    free(to_init);
    to_init = NULL;
  }

  return NULL; /* Failure. */
} /* intern__fre__init_pmatch_node() */


/* Init all nodes of a pmatch_table linked-list. */
fre_pmatch* intern__fre__init_ptable(size_t numof_nodes)
{
  size_t i = 0;
  fre_pmatch *head = NULL;
  
  
  if (numof_nodes == 0){
    abort();
  }
  if ((head = intern__fre__init_pmatch_node()) == NULL){
    intern__fre__errmesg("Intern__fre__init_pmatch_node");
    abort();
  }
  head->headnode = head;

  while (i < numof_nodes){
    if ((head->next_match = intern__fre__init_pmatch_node()) == NULL){
      intern__fre__errmesg("Intern__fre__init_pmatch_node");
      abort();
    }
    head->next_match->headnode = head->headnode;
    head = head->next_match;
    ++i;
  }
  head = head->headnode;

  return head;
}
  

/* Release a single node of a pattern's pmatch_table. */
void intern__fre__free_pmatch_node(fre_pmatch *to_free)
{
  size_t i = 0;
  /*  If we're being passed a NULL argument, return now. */
  if (to_free != NULL){
    
    if (to_free->sub_match != NULL){
      for (i = 0; i < FRE_MAX_SUB_MATCHES; i++){
	if (to_free->sub_match[i] != NULL){
	  free(to_free->sub_match[i]);
	  to_free->sub_match[i] = NULL;
	}
      }
      free(to_free->sub_match);
      to_free->sub_match = NULL;
    }
    if (to_free->whole_match != NULL){
      free(to_free->whole_match);
      to_free->whole_match = NULL;
    }
    
    to_free->next_match = NULL;
    to_free->headnode = NULL;
    free(to_free);
  }
  return;
  

} /* intern__fre__free_pmatch_node() */


/* Free a complete pmatch_table. */
void intern__fre__free_ptable(fre_pmatch *pheadnode)
{
  fre_pmatch *temp;
  pheadnode = pheadnode->headnode;
  while (pheadnode != NULL){
    temp = pheadnode;
    pheadnode = pheadnode->next_match;
    intern__fre__free_pmatch_node(temp);
  }
  return;

} /* intern__fre__free_ptable() */


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
  freg_object->fre_p1_compiled = false;
  freg_object->fre_paired_delimiters = false;
  freg_object->delimiter = '\0';
  freg_object->c_delimiter = '\0';
  freg_object->fre_op_flag = NONE;
  freg_object->fre_op_bref = false;
  freg_object->operation = NULL;
  
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
  freg_object->operation = NULL;

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
 * MACRO - Add current token to the given stack and
 * update the given stack's TOS.
 */
#define FRE_PUSH(token, stack, tos) do {		\
    stack[*tos] = token;				\
    ++(*tos);						\
  } while (0) ;



/* Skip commentary tokens and adjust the pattern lenght afterward. */
int FRE_SKIP_COMMENTS(char *pattern,
		      size_t *pattern_len,
		      size_t *token_ind)
{
  size_t i = 0;							
  while((pattern[*token_ind] != '\n')					
	&& (pattern[*token_ind] != '\0')) {				
    ++(*token_ind);
    ++i;						
  }
  /* Adjust the pattern's lenght and check for overflow. */
  if (((*pattern_len) - i > 0)					
      && ((*pattern_len) - i < FRE_MAX_PATTERN_LENGHT)) {		
    (*pattern_len) -= i;						
  } else {								
    errno = 0;
    intern__fre__errmesg("FRE_SKIP_COMMENTS failed to adjust pattern's lenght"); 
    errno = EOVERFLOW;
    return FRE_ERROR;				
  }
  return FRE_OP_SUCCESSFUL;
} 



int FRE_FETCH_MODIFIERS(char *pattern,
			fre_pattern *freg_object,
			size_t *token_ind)
{
  while (pattern[*token_ind] != '\0') {
    switch (pattern[*token_ind]) {
    case 'g':
      freg_object->fre_mod_global = true;
      break;
    case 'i':
      freg_object->fre_mod_icase = true;
      break;
    case 's':
      freg_object->fre_mod_newline = true;
      break;
    case 'm':
      freg_object->fre_mod_boleol = true;
      break;
    case 'x':
      freg_object->fre_mod_ext = true;
      break;
    default:
      errno = 0;
      intern__fre__errmesg("Unknown modifier in pattern.");
      return FRE_ERROR;
    }
    (*token_ind)++;
  }
  /* 
   * Decrement token_ind to fix an off by 1 error where token_ind would end up 1 more than the
   * terminating NUL byte of pattern, which was still in the process' address space causing chaos.
   */
  --(*token_ind);
  
  return FRE_OP_SUCCESSFUL;
}

int FRE_HANDLE_BREF(char *pattern,
		    size_t *token_ind,
		    size_t sub_match_ind,
		    int fre_is_sub,
		    fre_pattern *freg_object)
{
  char refnum_string[FRE_MAX_PATTERN_LENGHT];
  long refnum = 0;
  size_t i = 0;
  freg_object->fre_op_bref = true;
  if (!fre_is_sub) {
    freg_object->backref_pos->in_pattern[freg_object->backref_pos->in_pattern_c] = sub_match_ind;
  }
  else {
    freg_object->backref_pos->in_substitute[freg_object->backref_pos->in_substitute_c] = sub_match_ind;
  }
  memset(refnum_string, 0, FRE_MAX_PATTERN_LENGHT);
  ++(*token_ind);
  do {
    refnum_string[i++] = pattern[(*token_ind)++];
  } while (isdigit(*token_ind));
  refnum = atol(refnum_string);
  if (!fre_is_sub){
    freg_object->backref_pos->p_sm_number[freg_object->backref_pos->in_pattern_c++] = refnum;
  }
  else{
    freg_object->backref_pos->s_sm_number[freg_object->backref_pos->in_substitute_c++] = refnum;
  }
  return FRE_OP_SUCCESSFUL;
}

/* 
 * Strip a pattern from all its Perl-like elements.
 * Separates "matching" and "substitute" patterns of a 
 * substitution or transliteration operation.
 */
int intern__fre__strip_pattern(char *pattern,
			       fre_pattern *freg_object,
			       size_t token_ind)
{
int    bref_num = 0;                    
 char   bref_num_string[FRE_MAX_SUB_MATCHES]; /* Used by atoi() to convert encountered bref numbers into integers. */
  size_t i = 0;
  size_t spa_tos = 0;                          /* ->striped_pattern[0|1]'s tos values. */
  size_t spa_ind = 0;                          /* 0: matching pattern ind; 1: substitute pattern ind. */
  size_t delimiter_pairs_c = 1;                /* Count of pairs of paired-delimiter seen.*/
  size_t numof_seen_delimiter = 1;             /*
						* We've been given the index of the first token 
						* following the first delimiter. 
						*/
  size_t numof_seen_tokens = 0;                /* To help registering positions of back-references. */
  size_t pattern_len = strnlen(pattern,FRE_MAX_PATTERN_LENGHT); /* The pattern's lenght. */
  
#define FRE_TOKEN pattern[token_ind]

  while(1){
    /* When reaching the end of pattern. */
    if (FRE_TOKEN == '\0'){
      if ((freg_object->fre_op_flag == MATCH
	   && (numof_seen_delimiter == FRE_EXPECTED_M_OP_DELIMITER))
	  || freg_object->fre_paired_delimiters == true){
	break;
      }
      /* End substitution/transliteration operations patterns. */
      else if (freg_object->fre_op_flag == SUBSTITUTE || freg_object->fre_op_flag == TRANSLITERATE){
	if ((numof_seen_delimiter == FRE_EXPECTED_ST_OP_DELIMITER)
	    ||freg_object->fre_paired_delimiters == true){
	  break;
	}
	else { /* Syntax error. */
	  errno = 0;
 	  intern__fre__errmesg("Encountered end of string before end of pattern");
	  return FRE_ERROR;
	}
      }
      else { /* Syntax error. */
	errno = 0;
	intern__fre__errmesg("Encountered end of string before end of pattern");
	return FRE_ERROR;
      }
    }
    /* 
     * Increment numof_seen_tokens here, and
     * decrement it when we find a backref.
     * The first backref's position is taken from the begining of string,
     * all following backrefs' positions are taken from the numof_seen_tokens
     * since the previously found backref.
     */
    ++numof_seen_tokens;

    /* 
     * When spa_ind gets bigger than 1, Fetch the patterns
     * modifiers and end _strip_pattern().
     */
    if (spa_ind > 1){
      FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
      break;
    }
    /* Handle patterns with paired-type delimiters. */
    if (freg_object->fre_paired_delimiters == true){
      /*
       * When the pairs counter reaches 0, in a match op pattern
       * we must push the next tokens if any to the modifier stack right now,
       * else we must populate striped_pattern[1] first, then push any remaining 
       * tokens to the modifiers stack. 
       */
      if (delimiter_pairs_c <= 0) {
	if (freg_object->fre_op_flag == MATCH){
	  /* NULL terminate the stripped off pattern. */
	  FRE_PUSH('\0', freg_object->striped_pattern[spa_ind], &spa_tos);
	  FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	  break;
	}
	/* Handle patterns of a SUBSTITUTE or TRANSLITERATE operation.*/
	else {
	  /* Handle delimiters for when the delimiter_pairs_c[ount] is 0. */
	  if(FRE_TOKEN == freg_object->delimiter){
	    if (spa_ind == 0) { continue; }
	    else if (spa_ind == 1) {
	      spa_tos = 0;
	      numof_seen_tokens = 0;
	      ++delimiter_pairs_c;
	      ++token_ind;
	      continue;
	    }
	  }
	  /* NULL terminate the spa_indexed striped_pattern. */
	  else if (FRE_TOKEN == freg_object->c_delimiter){
	    FRE_PUSH('\0', freg_object->striped_pattern[spa_ind], &spa_tos);
	    ++spa_ind;	    
	    continue;
	  }
	}
      }
      /* delimiter_pairs > 0 */
      else {
	/* Handle delimiters when delimiter_pairs_c is not 0. */
	if (FRE_TOKEN == freg_object->delimiter){
	  FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	  ++delimiter_pairs_c;
	  ++token_ind;                  /* Get next token. */
	  continue;
	}
	/* Found the end of a pair of delimiters. */
	else if (FRE_TOKEN == freg_object->c_delimiter){
	  --delimiter_pairs_c;
	  /* Push it to the pattern's stack if -> once decremented <- the pair counter is still > 0. */
	  if (delimiter_pairs_c > 0){
	    FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	  }
	  else{
	    spa_ind++;
	  }
	  ++token_ind;                  /* Get next token. */
	  continue;
	}
	/* Fetch the substitute pattern's sub-refs positions, if any, right now. */
	if ((spa_ind == 1) && (FRE_TOKEN == '$' || FRE_TOKEN == '\\')) {
	  if (!isdigit(pattern[token_ind + 1])) {
	    /* Let '$' be an anchor if the next token is not a digit. */
	    FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	    ++token_ind;
	    continue;
	  }
	  else {
	    --numof_seen_tokens; 
	    FRE_HANDLE_BREF(pattern,
			    &token_ind,
			    (freg_object->backref_pos->in_substitute_c == 0) ? spa_tos : numof_seen_tokens,
			    1, freg_object);
	    if (errno != 0) return FRE_ERROR;
	    numof_seen_tokens = 0;
	    continue; 
	  }
	}
	/* When token is not a backref marker. */
	FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);	
      }
    } 

    /* Handle non-paired delimiter type. */
    else {
      /* Handle delimiters. */
      if (FRE_TOKEN == freg_object->delimiter){
	/* Terminate pattern and adjust spa indexes to build the 2nd pattern, if any. */
	FRE_PUSH('\0', freg_object->striped_pattern[spa_ind], &spa_tos);
	++numof_seen_delimiter;
	++spa_ind;
	spa_tos = 0;
	++token_ind; /* Get next token. */
	numof_seen_tokens = 0;
	continue;
      }
      /* Handle matching operation patterns. */
      if (freg_object->fre_op_flag == MATCH){
	if (numof_seen_delimiter < FRE_EXPECTED_M_OP_DELIMITER){
	  FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	}
	else if (numof_seen_delimiter == FRE_EXPECTED_M_OP_DELIMITER){
	  FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	}
	else {
	  /* Syntax error. */
	  errno = 0;
	  intern__fre__errmesg("Too many delimiters, forgot a backslash maybe?");
	  return FRE_ERROR;
	}
      }
      /* Handle substitution and transliteration operations patterns. */
      else {
	/* We've yet to encounter the expecte amount of delimiters. */
	if (numof_seen_delimiter < FRE_EXPECTED_ST_OP_DELIMITER) {
	  /* Handle a back-reference. */
	  if (spa_ind == 1 && (FRE_TOKEN == '$' || FRE_TOKEN == '\\')){
	    if (isdigit(pattern[token_ind + 1])){
	      --numof_seen_tokens;
	      FRE_HANDLE_BREF(pattern,
			      &token_ind,
			      (freg_object->backref_pos->in_substitute_c == 0) ? spa_tos : numof_seen_tokens,
			      1, freg_object);
	      if (errno != 0) return FRE_ERROR;
	      numof_seen_tokens = 0;
	      continue;
	    }
	    /* 
	     * Let '$' be an anchor, push it on the pattern's stack
	     * when the next token is not a digit.
	     */
	    else {
	      FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	      ++token_ind;
	      continue;
	    }
	  }
	  /* When token is not a backref marker. */
	  FRE_PUSH(FRE_TOKEN, freg_object->striped_pattern[spa_ind], &spa_tos);
	}
	/* 
	 * Fetch the pattern's modifier(s) when the expected amount of
	 * delimiters have been parsed.
	 */
	else if (numof_seen_delimiter == FRE_EXPECTED_ST_OP_DELIMITER) {
	  FRE_FETCH_MODIFIERS(pattern, freg_object, &token_ind);
	}
	else {
	  /* Syntax error. */
	  errno = 0;
	  intern__fre__errmesg("Too many delimiters, forgot a backslash maybe?");
	  return FRE_ERROR;
	}
      }
    }

    /* If we did not hit a continue statement yet, get next token. */
    ++token_ind;
  } /* while(1) */

  return FRE_OP_SUCCESSFUL;
  
} /* intern__fre__strip_pattern() */


/*
 * This MACRO is used to verify each escape sequence found in the user's given pattern,
 * and replace any sequences not supported by the POSIX standard. 
 * Takes the parsed pattern, the token index of the backslash leading the escape sequence,
 * the new pattern made by _perl_to_posix, the new pattern's top of stack, the new pattern's
 * current lenght and the fre_pattern object used throughout the library.
 */
#define FRE_CERTIFY_ESC_SEQ(pattern, token_ind, new_pat, new_pat_tos, new_pat_len, freg_object) do { \
    size_t i = 0;							\
    switch(pattern[*token_ind + 1]){					\
    case 'd':								\
      if ((*new_pat_len += strlen(FRE_POSIX_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Could not convert the Perl-like digit range '\\d'"); \
	return FRE_ERROR;						\
      }									\
      while (FRE_POSIX_DIGIT_RANGE[i] != '\0'){				\
	FRE_PUSH(FRE_POSIX_DIGIT_RANGE[i++], new_pat, new_pat_tos);	\
      }									\
      /* Adjust the token index. */					\
      (*token_ind) += 2;						\
      break;								\
    case 'D':								\
      if ((*new_pat_len += strlen(FRE_POSIX_NON_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Could not convert the Perl-like not digit range '\\D'"); \
	return FRE_ERROR;						\
      }									\
      while(FRE_POSIX_NON_DIGIT_RANGE[i] != '\0') {			\
	FRE_PUSH(FRE_POSIX_NON_DIGIT_RANGE[i++], new_pat, new_pat_tos)	\
      }									\
      /* Adjust the token index. */					\
      (*token_ind) += 2;						\
      break;								\
      /* More test will go here. */					\
    default:								\
      /* Assume a supported sequence, push the tokens on the pattern stack. */ \
      FRE_PUSH(pattern[(*token_ind)++], new_pat, new_pat_tos);		\
      FRE_PUSH(pattern[(*token_ind)++], new_pat, new_pat_tos);		\
      /*(*token_ind) += 2;*/						\
      break;								\
    }									\
  }while (0);

/* 
 * Convert any non-POSIX, Perl-like elements into
 * POSIX supported constructs.
 */
int intern__fre__perl_to_posix(fre_pattern *freg_object){
  bool   call_handle_bref = false;         /* True when _certify_esq_seq found digits after a backslash. */
  size_t numof_seen_tokens = 0;            /* Used to help register back-reference positions. */
  size_t token_ind = 0;
  size_t new_pattern_tos = 0;              /* new_pattern's top of stack. */
  size_t new_pattern_len = strnlen(freg_object->striped_pattern[0], FRE_MAX_PATTERN_LENGHT);
  char   new_pattern[FRE_MAX_PATTERN_LENGHT]; /* The array used to build the new pattern. */

#undef FRE_TOKEN
#define FRE_TOKEN freg_object->striped_pattern[0][token_ind]

  while(1) {
    if (FRE_TOKEN == '\0'){
      /* Terminate the new pattern. */
      FRE_PUSH('\0', new_pattern, &new_pattern_tos);
      break;
    }
    /*
     * Increment numof_seen_tokens now, decrement it
     * only when we're sure we found a backref. 
     */
    ++numof_seen_tokens;
  
    if (FRE_TOKEN == '\\' || FRE_TOKEN == '$'){
      /* Must be a back-reference. */
      if (isdigit(freg_object->striped_pattern[0][token_ind + 1])){
	--numof_seen_tokens;
	FRE_HANDLE_BREF(freg_object->striped_pattern[0],
			&token_ind,
			(freg_object->backref_pos->in_pattern_c == 0) ? new_pattern_tos : numof_seen_tokens,
			0, freg_object);
      	if (errno != 0) return FRE_ERROR;
	numof_seen_tokens = 0;
	continue;
      }
      /* Must be an escape sequence. */
      else if (FRE_TOKEN == '\\'){
	FRE_CERTIFY_ESC_SEQ(freg_object->striped_pattern[0], &token_ind, new_pattern,
			    &new_pattern_tos, &new_pattern_len, freg_object);
	continue;
      }
      else {
	FRE_PUSH(FRE_TOKEN, new_pattern, &new_pattern_tos);
      }
    }

    else if (freg_object->fre_mod_ext == true){
      if (isspace(FRE_TOKEN)) {
	++token_ind;
	continue;
      }
      else if (FRE_TOKEN == '#'){
	/* 
	 * Skip tokens in the unmodified pattern, adjust the
	 * lenght of the new_pattern, itself based from the lenght of
	 * the unmodified pattern.
	 */
	FRE_SKIP_COMMENTS(freg_object->striped_pattern[0],
			  &new_pattern_len,
			  &token_ind);
      }
      /* Current token is not special. */
      else {
	FRE_PUSH(FRE_TOKEN, new_pattern, &new_pattern_tos);
	++token_ind;
	continue;
      }
    }

    /* Valid token. */
    else {
      FRE_PUSH(FRE_TOKEN, new_pattern, &new_pattern_tos);
    }
    /* Get next token. */
    ++token_ind;
  } /* while(1) */

  /* 
   * Replace the matching pattern with the newly created one and
   * make a copy of both striped-pattern pattern into the object's saved_pattern array. 
   */
  if (SU_strcpy(freg_object->striped_pattern[0], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  if (SU_strcpy(freg_object->saved_pattern[0], freg_object->striped_pattern[0], FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return FRE_ERROR;
  }
  if (freg_object->striped_pattern[1][0] != '\0'){
    if (SU_strcpy(freg_object->saved_pattern[1], freg_object->striped_pattern[1], FRE_MAX_PATTERN_LENGHT) == NULL){
      intern__fre__errmesg("SU_strcpy");
      return FRE_ERROR;
    }
  }
  
  return FRE_OP_SUCCESSFUL;
  
} /* intern__fre__perl_to_posix() */




/* Insert all sub-matches into the given pattern. */
char* intern__fre__insert_sm(fre_pattern *freg_object,      /* The object used throughout the library. */
			     char *string,                  /* The string to match. */
			     size_t is_sub)             /* 0 = matching pattern, 1 substitute pattern. */

{
  int bcount = 0;             /* The index of the ->in_pattern[] backref we're working on. */
  int string_ind = 0;         /* Index of the current sub-match token within the caller's string. */
  int prev_sm_lenght = 0;     /* Lenght of the previously substituted sub-match. */

  int np_ind = 0, sp_ind = 0; /* new_pattern's index, striped_pattern[is_sub]'s index. */
  char new_pattern[FRE_MAX_PATTERN_LENGHT];

  memset(new_pattern, 0, FRE_MAX_PATTERN_LENGHT);
  while(sp_ind <= FRE_MAX_PATTERN_LENGHT) {
    if (freg_object->striped_pattern[is_sub][sp_ind] == '\0'
	&& freg_object->backref_pos->in_pattern[bcount] == -1)
      break;
    
    if (bcount == 0){
      if (freg_object->backref_pos->in_pattern[bcount] != -1) {
	if (sp_ind == freg_object->backref_pos->in_pattern[bcount]){
	  for (string_ind = fre_pmatch_table->sub_match[(freg_object->backref_pos->p_sm_number[bcount] - 1)]->bo;
	       string_ind < fre_pmatch_table->sub_match[(freg_object->backref_pos->p_sm_number[bcount] - 1)]->eo;
	       string_ind++) {
	    new_pattern[np_ind++] = string[string_ind];
	    ++prev_sm_lenght; /* Set the lenght of the sub-match. */
	  }
	  /* Get next backref */
	  ++bcount;
	  continue;
	}
	else {
	  new_pattern[np_ind++] = freg_object->striped_pattern[is_sub][sp_ind++];
	  continue;
	}
      }
      else {
	/* 
	 * Note that ->in_pattern[bcount] is -1. Should get caught on first itteration if it's to happen ~ . 
	 * Return the pattern unmodified.
	 */
	return freg_object->striped_pattern[is_sub]; 
      }
    }
    /* Bcount > 0 */
    if (freg_object->backref_pos->in_pattern[bcount] != -1){
      /* 
       * Add the lenght of the previous sub-match replacement to 
       * the offset ->in_pattern[bcount] to find where in ->striped_pattern to replace
       * the next backref.
       */
      if (sp_ind == (prev_sm_lenght + freg_object->backref_pos->in_pattern[bcount])){
	prev_sm_lenght = 0;
	for (string_ind = fre_pmatch_table->sub_match[(freg_object->backref_pos->p_sm_number[bcount] - 1)]->bo;
	     string_ind <= fre_pmatch_table->sub_match[(freg_object->backref_pos->p_sm_number[bcount] - 1)]->eo;
	     string_ind++) {
	  new_pattern[np_ind++] = string[string_ind];
	  ++prev_sm_lenght;
	}
	/* Get next backref. */
	++bcount;
	continue;
      }
    }
    new_pattern[np_ind++] = freg_object->striped_pattern[is_sub][sp_ind++];
  } /* while( freg_object->striped_patern.... */
  if (SU_strcpy(freg_object->striped_pattern[is_sub], new_pattern, FRE_MAX_PATTERN_LENGHT) == NULL){
    intern__fre__errmesg("SU_strcpy");
    return NULL;
  }

  return freg_object->striped_pattern[is_sub];
  
} /* intern__fre__insert_sm() */


/*
 * Remove a character sequence from a NUL terminated string. 
 * No error checks are made. 
 * Self warning >>> Possible comparaison againt signed vs unsigned int. <<<

 Add a parameter: numof_token_skipped. Then modify _match_op() to add this 
 offset to each match/sub-match position. this should fix the current
 position misalignment (or whatever it's called :) 

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
  if (SU_strcpy(string, new_string, string_size) == NULL){
    intern__fre__errmesg("SU_strcpy");
    if (new_string)
      free(new_string);
    return NULL;
  }
  if (new_string)
    free(new_string);
  return string;

} /* intern__fre__cut_match() */
  
/* 
 * Debug hook
 * Print all values of a given fre_pattern object.
 */
void print_pattern_hook(fre_pattern* pat)
{
  size_t i = 0;
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
  fprintf(stderr, "Operation type: %s\nfre_op_bref %s\n",
	  ((pat->fre_op_flag == NONE) ? "NONE" :
	   (pat->fre_op_flag == MATCH) ? "MATCH" :
	   (pat->fre_op_flag == SUBSTITUTE) ? "SUBSTITUTE" :
	   (pat->fre_op_flag == TRANSLITERATE) ? "TRANSLITERATE" : "Error, invalid flag"),
	  ((pat->fre_op_bref == true) ? "true" : "false"));
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
  fprintf(stderr,"\noperation %s\ncomp_pattern %s\n",
	  ((pat->operation != NULL) ? "Defined" : "NULL"),
	  ((pat->comp_pattern != NULL) ? "Defined" : "NULL"));
  fprintf(stderr, "striped_pattern[0] %s\nstriped_pattern[1] %s\n",
	  pat->striped_pattern[0], pat->striped_pattern[1]);
  fprintf(stderr, "saved_pattern[0] %s\nsaved_pattern[1] %s\n\n",
	  pat->saved_pattern[0], pat->saved_pattern[1]);
  pthread_mutex_unlock(&fre_stderr_mutex);
  return;
}

void print_string_hook(char* s)
{
  if (s == NULL)
    return;
  else
    fprintf(stderr, "%s\n", s);
}






/* 
 * Gather all pattern modifiers and raise their
 * respective flags in the fre_pattern object. 
 *
 #define FRE_FETCH_MODIFIERS(pattern, freg_object, token_ind) do {	\
 while(pattern[*token_ind] != 0){					\
 switch(pattern[*token_ind]){						\
 case 'g':								\
 freg_object->fre_mod_global = true;  break;				\
 case 'i':								\
 freg_object->fre_mod_icase = true;   break;				\
 case 's':								\
 freg_object->fre_mod_newline = true; break;				\
 case 'm':								\
 freg_object->fre_mod_boleol = true;  break;				\
 case 'x':								\
 freg_object->fre_mod_ext = true;     break;				\
 default:								\
 errno = 0; intern__fre__errmesg("Unknown modifier in pattern");	\
 return FRE_ERROR;							\
 }									\
 (*token_ind)++;							\
 }									\
 } while (0);								\
*/
