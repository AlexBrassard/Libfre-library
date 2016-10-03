/*
 *
 *
 *  Libfre  -  Initialization and termination routines.
 *  Version:   0.010
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "fre_internal.h"

static inline fre_pmatch* intern__fre__fetch_head(void);
pthread_mutex_t fre_stderr_mutex;  /* For when error/debug messages has multiple function calls. */
pthread_key_t pmatch_table_key;    /* Keys to the pmatch_table kindom. */
fre_headnodes *fre_headnode_table; /* To keep track of allocated pmatch_tables. */

#define pmatch_table (intern__fre__pmatch_location())

int __attribute__ ((constructor)) intern__fre__lib_init(void)
{
  /* Initialize a mutex to serialize all stderr messages. */
  if (pthread_mutex_init(&fre_stderr_mutex, NULL) != 0){
    perror("Pthread_mutex_init");
    return FRE_ERROR;
  }
  /* Initialize a pthread key to make pmatch_tables thread specific. */
  if (pthread_key_create(&pmatch_table_key, NULL) != 0){
    perror("Pthread_key_create");
    return FRE_ERROR;
  }
  /* Initialize the global table of headnode pointers. */
  if ((fre_headnode_table = intern__fre__init_head_table()) == NULL){
    perror("intern__fre__init_head_table");
    return FRE_ERROR;
  }
  
  return FRE_OP_SUCCESSFUL;
}

void __attribute__ ((destructor)) intern__fre__lib_finit(void)
{
  fre_pmatch *to_free = NULL;
  size_t i = 0;
  /* Deallocate stderr's mutex. */
  if (pthread_mutex_destroy(&fre_stderr_mutex) != 0){
    /* Say what failed but still try to free what's left. */
    perror("Pthread_mutex_destroy");
  }

  /* Free the global headnode table itself. */
  intern__fre__free_head_table();//fre_headnode_table);

  if (pthread_key_delete(pmatch_table_key) != 0){
    perror("Pthread_key_delete");
  }

  
} /* intern__fre__pmatch_location() */


void intern__fre__key_delete(void *key)
{
  fre_pmatch *to_free = NULL;
  
  //  if ((to_free = pmatch_table )!= NULL)
  //    intern__fre__free_ptable(to_free);
  if (key != NULL)
    free(key);
  key = NULL;
}

/*
 * A pmatch_table is a structure containing possible 
 * positions of sub-matches within the string argument of fre_bind().
 * Pmatch_tables are thread-specific and their content gets
 * overwritten everytime a user's thread(s) calls fre_bind().

 * When a non-NULL argument has been passed, set the thread specific 
 * pointer to point to that value and return it.
 */
fre_pmatch* intern__fre__pmatch_location(fre_pmatch *new_head)
{
  size_t i = 0;
  fre_pmatch *table = NULL;
  if (new_head == NULL){
    /* If there's an existing table, fetch and return it. */
    if ((table = pthread_getspecific(pmatch_table_key)) != NULL){
      return table;
    }
    else {
      pthread_mutex_lock(fre_headnode_table->table_lock);
      if ((table = intern__fre__fetch_head()) == NULL){
	intern__fre__errmesg("Intern__fre__fetch_head");
	return NULL;
      }
      pthread_mutex_unlock(fre_headnode_table->table_lock);
      if (pthread_setspecific(pmatch_table_key, table) != 0){
	intern__fre__errmesg("Pthread_setspecific");
	return NULL;
      }
      if ((table = pthread_getspecific(pmatch_table_key)) == NULL){
	intern__fre__errmesg("Pthread_getspecific");
	return NULL;
      }
      return table;
    }
  }
  else {
    if (pthread_setspecific(pmatch_table_key, new_head) != 0){
      intern__fre__errmesg("Pthread_set_specific");
      return NULL;
    }
    if ((table = pthread_getspecific(pmatch_table_key)) == NULL){
      intern__fre__errmesg("Pthread_getspecific");
      return NULL;
    }
    return table;
  }
} /* intern__fre__pmatch_location() */

static inline fre_pmatch* intern__fre__fetch_head(void)
{
  if (fre_headnode_table->head_list_tos == 0){
    errno = 0;
    intern__fre__errmesg("Too many requests");
    return NULL;
  }
  return fre_headnode_table->head_list[fre_headnode_table->head_list_tos--];
}

