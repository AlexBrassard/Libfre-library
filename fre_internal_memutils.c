/*
 *
 *  Libfre library  -  Memory management functions.
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#include "fre_internal.h"
#include "fre_internal_macros.h"
#include "fre_internal_errcodes.h"

extern fre_headnodes *fre_headnode_table;


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
  to_init->ls_object = NULL;
  to_init->subm_per_match = 0;
  to_init->lastop_retval = 0;
  to_init->fre_saved_object = false;
  to_init->wm_ind = 0;
  to_init->sm_ind = 0;
  to_init->wm_size = FRE_MAX_MATCHES;
  to_init->sm_size = FRE_MAX_SUB_MATCHES;

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
  freg_object->bref_to_insert = 0;
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
