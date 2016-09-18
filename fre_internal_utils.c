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
fre_pmatch* intern__fre__init_pmatch_node(fre_pmatch *headnode) /* Headnode can be NULL. */
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
  to_init->headnode = headnode;

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


/* Release a single node of a pattern's pmatch_table. */
void intern__fre__free_pmatch_node(fre_pmatch *to_free)
{
  size_t i = 0;
  /*  If we're being passed a NULL argument, return now. */
  if (to_free == NULL) return;

  if (to_free->whole_match != NULL){
    free(to_free->whole_match);
    to_free->whole_match = NULL;
  }
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
  to_free->next_match = NULL;
  to_free->headnode = NULL;
  free(to_free);
  to_free = NULL;
  
  return;

} /* intern__fre__free_pmatch_node() */


/* Free a complete pmatch_table. */
void intern__fre__free_ptable(fre_pmatch *headnode)
{
  fre_pmatch *temp = NULL;

  while (headnode != NULL){
    temp = headnode;
    headnode = headnode->next_match;
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
