/*
 *
 *
 *  Libfre library  -  Public header file.
 *  Version   0.600
 *
 *
 */


#ifndef FRE_PUBLIC_HEADER
# define FRE_PUBLIC_HEADER

/** Function prototype **/

int fre_bind(char *pattern,            /* The regex pattern. */
	     char *string,             /* The string to bind the pattern against. */
	     size_t string_size);      /* The string's size (not its lenght). */

void FRE_PERROR(char *funcname);       /* Library's error messages. */
#endif /* FRE_PUBLIC_HEADER */
