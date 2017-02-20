/*
 *
 *  Libfre  -  Internal error codes and corresponding messages.
 *
 */
#include <errno.h>
#include <stdio.h>
#include <pthread.h>

#include "fre_internal.h"

#ifndef FRE_INTERNAL_ERROR_CODES
# define FRE_INTERNAL_ERROR_CODES


# define FRE_MAX_ERRMESG_LEN 100

extern pthread_mutex_t fre_stderr_mutex;


/* Specific error codes and corresponding messages. */
enum fre_errcodes {
  FRE_LENGHTADJ = 200,
  FRE_INVALMODIF,
  FRE_QUOTEOVERF,
  FRE_BOUNDOVERF,
  FRE_ESCSEQOVERF,
  FRE_PATRNDELIM,
  FRE_PATRNPDELIM,
  FRE_MISPLACCHAR,
  FRE_UNTERMBRAKT,
  FRE_UNKNOWNOP,
  FRE_INVALBREF,
  FRE_INVALTRANSL,
  FRE_PATRNTOOLONG,
  FRE_OPERROR
  
};
 
static const char fre_errmesg[][FRE_MAX_ERRMESG_LEN] ={
  "Failed to adjust pattern's lenght",
  "Unknown modifier in pattern",
  "Exceeding POSIX lenght limit replacing Perl-like \\Q..\\E sequence",
  "Exceeding POSIX lenght limit replacing a Perl-like word boundary sequence",
  "", /* To keep fre_errcodes enums aligned with this array. */
  "Unexpected number of delimiters in the given pattern",
  "Unexpected number of delimiter pairs in the given pattern",
  "Unexpected character in between the given 'matching' and 'substitute' patterns",
  "Unterminated bracket expression in the given pattern",
  "Unknown operation in the given pattern",
  "No valid position in regmatch_t array[0]",
  "Non matching number of characters in the given transliteration pattern",
  "Libfre takes pattern no longer than 256 bytes including a NULL byte to stay POSIX conformant",
  "Error executing the requested operation"
};


/* Simple error message. */
# define intern__fre__errmesg(funcname) do {				\
    pthread_mutex_lock(&fre_stderr_mutex);				\
    /* Library's error codes begins at 200 and up. */			\
    if (errno && errno < 200) {						\
      perror(funcname);							\
    } else if (errno >= 200) {						\
      /* funcname represents the faulty Perl-like construct when errno is FRE_ESCSEQOVERF. */ \
      if (errno == FRE_ESCSEQOVERF){					\
	fprintf(stderr, "FRE_CERTIFY_ESC_SEQ - Exceeding POSIX lenght limit converting Perl-like '%s' into a POSIX construct", \
		funcname);						\
      } else {								\
	fprintf(stderr, "%s - %s", funcname, fre_errmesg[(errno-200)]);	\
      }									\
    } else {								\
      pthread_mutex_unlock(&fre_stderr_mutex);				\
      break;								\
    }									\
    fprintf(stderr, "\n\n");						\
    pthread_mutex_unlock(&fre_stderr_mutex);				\
  } while (0);
			     
#endif /* FRE_INTERNAL_ERROR_CODES */
