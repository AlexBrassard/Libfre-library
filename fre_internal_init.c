/*
 *
 *
 *  Libfre  -  Initialization and termination routines.
 *  Version:   0.600
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "fre_internal.h"
#include "fre_internal_errcodes.h"

static inline fre_pmatch* intern__fre__fetch_head(void);
pthread_mutex_t fre_stderr_mutex;  /* For when error/debug messages has multiple function calls. */
pthread_key_t pmatch_table_key;    /* Keys to the pmatch_table kindom. */
fre_headnodes *fre_headnode_table; /* To keep track of allocated pmatch_tables. */


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
  /* Initialize the global table of allocated pmatch_tables. */
  if ((fre_headnode_table = intern__fre__init_head_table()) == NULL){
    perror("intern__fre__init_head_table");
    return FRE_ERROR;
  }
  
  return FRE_OP_SUCCESSFUL;
}

void __attribute__ ((destructor)) intern__fre__lib_finit(void)
{

  if (pthread_mutex_destroy(&fre_stderr_mutex) != 0){
    /* Say what failed but still try to free what's left. */
    perror("Pthread_mutex_destroy");
  }

  intern__fre__free_head_table();

  if (pthread_key_delete(pmatch_table_key) != 0){
    perror("Pthread_key_delete");
  }

} /* intern__fre__lib_finit() */


/* Because pthread_key_create needs a way to free the allocated key. */
void intern__fre__key_delete(void *key)
{
  if (key != NULL)
    free(key);
}

/*
 * Do not use this function directly, instead use the
 * fre_pmatch_table MACRO defined near the bottom of fre_internal.h .

 * A pmatch_table is a structure containing possible 
 * positions of matches and sub-matches within the string argument of fre_bind().
 * Pmatch_tables are thread-specific and their content gets
 * overwritten everytime a user's thread(s) calls _match_op().
 */
fre_pmatch* intern__fre__pmatch_location(void)
{
  fre_pmatch *table = NULL;
  /* If there's an existing table, fetch and return it. */
  if ((table = pthread_getspecific(pmatch_table_key)) != NULL){
    return table;
  }
  /* Else get one from the global headnode_table. */
  else {
    pthread_mutex_lock(fre_headnode_table->table_lock);
    if ((table = intern__fre__fetch_head()) == NULL){
      intern__fre__errmesg("Intern__fre__fetch_head");
      pthread_mutex_unlock(fre_headnode_table->table_lock);
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
  
} /* intern__fre__pmatch_location() */


/* 
 * Warning: The headnode_list's mutex MUST be locked before
 *          making a call to this function AND unlocked
 *          after the function returned. 
 */
static inline fre_pmatch* intern__fre__fetch_head(void)
{
  size_t new_size = 0, i = 0;
  fre_pmatch **new_head_list = NULL;

  /* Extend the list when we're out of tables. */
  if (fre_headnode_table->head_list_tos == fre_headnode_table->sizeof_table - 1){
    new_size = fre_headnode_table->sizeof_table * 2;
    if ((new_head_list = realloc(fre_headnode_table->head_list, new_size * sizeof(fre_pmatch*))) == NULL){
      intern__fre__errmesg("Realloc");
      return NULL;
    }
    for(i = fre_headnode_table->sizeof_table;
	i < new_size;
	i++) {
      if ((new_head_list[i] = intern__fre__init_pmatch_table()) == NULL){
	intern__fre__errmesg("Intern__fre__init_pmatch_table");
	goto errjmp;
      }
    }
    fre_headnode_table->head_list = new_head_list;
    fre_headnode_table->sizeof_table = new_size;
  }
  return fre_headnode_table->head_list[fre_headnode_table->head_list_tos++];

 errjmp:
  if (new_head_list){
    for (i = fre_headnode_table->sizeof_table; i < new_size; i++){
      if (new_head_list[i]){
	free(new_head_list[i]);
	new_head_list[i] = NULL;
      }
    }
    free(new_head_list);
    new_head_list = NULL;
  }
  return NULL;
}

