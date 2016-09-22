/*
 *
 *
 *  Libfre  -  Internal utilities.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <regex.h>
#include <errno.h>

#include "fre_internal.h"

extern fre_headnodes *fre_headnode_table;
//static inline fre_pmatch* intern__fre__fetch_head(void);
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
 * Fetch a pmatch_table headnode from the global headnode_table. 
 * _pmatch_location() is responsible of locking/unlocking the headnode_table's
 * mutex before and after the call to _fetch_head(). 
 * Use with care.
 
static inline fre_pmatch* intern__fre__fetch_head(void)
{
  if (fre_headnode_table->head_list_tos == 0){
    errno = 0;
    intern__fre__errmesg("Too many requests");
    return NULL;
  }
  return fre_headnode_table->head_list[fre_headnode_table->head_list_tos--];
  }*/
  


/* Possibly Obsolete.

 * Add a pointer to the headnode of the given linked-list
 * in the global table of headnodes fre_headnode_table.
 * Extend the global table if needed.
 */
int intern__fre__push_head(fre_pmatch *head)
{
  fre_pmatch **new_head_list = NULL;       /* In case we need more memory. */
  size_t temp_size = 0;
  /* 
   * Stop everything if the global table is NULL,
   * it would either mean the library's initialization had a problem
   * or that the table got somehow NULLed since the initialization.
   */
  if (fre_headnode_table == NULL){
    abort();
  }
  pthread_mutex_lock(fre_headnode_table->table_lock);
  /* If we reach the current table's boundaries, double them up. */
  if (fre_headnode_table->numof_tables == fre_headnode_table->sizeof_table){
    temp_size = (fre_headnode_table->sizeof_table * 2);
    if ((new_head_list = realloc(fre_headnode_table->head_list, temp_size * sizeof(fre_pmatch*))) == NULL){
      intern__fre__errmesg("Realloc");
      pthread_mutex_unlock(fre_headnode_table->table_lock);
      return FRE_ERROR;
    }
    fre_headnode_table->sizeof_table = temp_size;
    fre_headnode_table->head_list = new_head_list;
  }
  /* Add the given argument pointer to the head_list. */
  fre_headnode_table->head_list[fre_headnode_table->numof_tables++] = head;
  pthread_mutex_unlock(fre_headnode_table->table_lock);

  return FRE_OP_SUCCESSFUL;

}/* intern__fre__push_head() */


/* Free memory of all linked-list sitting in the global headnode_table. */
void intern__fre__clean_head_table(void)
{
  size_t i = 0;

  pthread_mutex_lock(fre_headnode_table->table_lock);
  if(fre_headnode_table == NULL)
    return;
  for (i = 0; i < fre_headnode_table->numof_tables; i++){
    if(fre_headnode_table->head_list[i] != NULL){
      intern__fre__free_ptable(fre_headnode_table->head_list[i]);
      fre_headnode_table->head_list[i] = NULL;
    }
  }
  pthread_mutex_unlock(fre_headnode_table->table_lock);
  return;
}

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
  if ((to_init->in_pattern = calloc(FRE_MAX_SUB_MATCHES, sizeof(int))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjump;
  }
  if ((to_init->in_substitute = calloc(FRE_MAX_SUB_MATCHES, sizeof(int))) == NULL){
    intern__fre__errmesg("Calloc");
    goto errjump;
  }
  to_init->in_pattern_c = 0;
  to_init->in_substitute_c = 0;
  return to_init;

 errjump:
  if (to_init != NULL){
    if (to_init->in_pattern != NULL){
      free(to_init->in_pattern);
      to_init->in_pattern = NULL;
    }
    if (to_init->in_substitute != NULL){
      free(to_init->in_substitute);
      to_init->in_substitute = NULL;
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
  if (to_free->in_substitute != NULL){
    free(to_free->in_substitute);
    to_free->in_substitute = NULL;
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
    if (freg_object->fre_p1_compiled == true)
      regfree(freg_object->comp_pattern);
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
  freg_object->operation = NULL;

  free(freg_object);

  return;
}


/** Regex Parser Utility Functions. **/

/*
 * MACRO - Add current token to the given stack and
 * update the given stack's TOS.
 */
#define intern__fre__push(token, stack, tos) do {	\
    stack[*tos] = token;				\
    ++(*tos);						\
  } while (0) ;


/* 
 * Skip all tokens until a newline or a NULL is found, 
 * then trim the pattern's lenght by the ammount of tokens we just skipped.
 */
int intern__fre__skip_comments(char *pattern,
			       size_t *pattern_len,
			       size_t *token_ind)
{
  size_t i = 0;                              /* Number of tokens skipped. */




  /* HAVE NOT BEEN TESTED ONCE YET */






  while(pattern[*token_ind] != '\n'
	&& pattern[*token_ind] != '\0'){
    ++(*token_ind);
    ++i;
  }
  /* Adjust the pattern's lenght, first check for overflow. */
  if (((*pattern_len) - i) > 0
      && ((*pattern_len) - i) < FRE_MAX_PATTERN_LENGHT){
    (*pattern_len) -= i;
  }
  else {
    errno = 0; /* Make sure errno is not set, else _errmesg will call perror() */
    intern__fre__errmesg("Intern__fre__skip_comments failed to adjust pattern's lenght");
    errno = EOVERFLOW;
    return FRE_ERROR;
  }
  return FRE_OP_SUCCESSFUL;

} /* intern__fre__skip_comments() */





/* 
 * Debug hook
 * Print all values of a given fre_pattern object.
 */
void print_pattern_hook(fre_pattern* pat)
{
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
  fprintf(stderr, "backref_pos %s\noperation %s\ncomp_pattern %s\n",
	  ((pat->backref_pos != NULL) ? "Defined" : "NULL"),
	  ((pat->operation != NULL) ? "Defined" : "NULL"),
	  ((pat->comp_pattern != NULL) ? "Defined" : "NULL"));
  fprintf(stderr, "striped_pattern[0] %s\nstriped_pattern[1] %s\n\n",
	  pat->striped_pattern[0], pat->striped_pattern[1]);
  pthread_mutex_unlock(&fre_stderr_mutex);
  return;
}
