/*
 *
 *  Libfre library  -  Internal header file.
 *
 */


#ifndef FRE_INTERNAL_HEADER
# define FRE_INTERNAL_HEADER 

# include <stdio.h>
# include <stdlib.h>
# include <stdbool.h>
# include <limits.h>
# include <errno.h>
# include <regex.h>
# include <pthread.h>

/** Data structures **/

/* Sub-match(es) begining/ending of match offsets structure. */
typedef struct fre_sm{
  int                   bo;               /* Offset of the begining of sub-match. */
  int                   eo;               /* Offset of the ending of sub-match. */

} fre_smatch;

/* Per-thread global sub-match table kept between invocations of fre_bind(). */
typedef struct fre_pmatch_tab {
  bool                  fre_modif_global; /* True when /g is activated, pmatch_table will contain extra nodes. */
  /* Determined by the values in regmatch_t[]. */
  fre_smatch            *whole_match;     /* Begining/ending of the complete match operation in the matched string. */
  fre_smatch            **sub_match;      /* Array of sub-match offsets. */
  struct fre_pmatch_tab *next_match;      /* Pointer to the node containing positions of the next match. */
  struct fre_pmatch_tab *headnode;        /* Pointer to the pmatch_table's head_node, for easy access. */
  
} fre_pmatch;


/* To keep track of where in each patterns are the back-references. */
typedef struct fre_brefs {
  int                   *in_pattern;    /* 
					 * [0] is relative to the begining of stripped_pattern[0],
					 * [1+] are relative to the begining of their previous element.
					 * (3 rel 2, 2 rel 1, 1 rel 0).
					 */
  int                   *in_substitute;  /*
					  * [0] is relative to the begining of stripped_pattern[1],
					  * [1+] are relative to the begining of their previous element.
					  */
  int                   in_pattern_c;    /* First free empty element of in_pattern. */
  int                   in_substitute_c; /* First free empty element of in_substitute. */

} fre_backref;

/* Flag to indicate a fre_pattern's operation. */
typedef enum foperation {
  NONE = -1,
  MATCH = 0,
  SUBSTITUTE,
  TRANSLITERATE

} fre_op_f;

/* Structure of a fre_pattern. */
typedef struct fpattern {
  /* Pattern modifiers */
  bool     fre_mod_boleol;         /* True when the '/m' modifier is activated. */
  bool     fre_mod_newline;        /* True when the '/s' modifier is activated. */
  bool     fre_mod_icase;          /* True when the '/i' modifier is activated. */
  bool     fre_mod_ext;            /* True when the '/x' modifier is activated. */
  bool     fre_mod_global;         /* True when the '/g' modifier is activated. */

  /* Indicate whether or not to regfree() the pattern. */
  bool     fre_p1_compiled;        /* True when stripped_pattern[0] has been regcomp()'d. */

  /* Pattern's delimiter(s) */
  bool     fre_paired_delimiters;  /* True when delimiter is one of  { ( [ <  */
  char     delimiter;              /* The delimiter used in the pattern. */
  char     c_delimiter;            /* Closing delimiter, when using paired-type delimiters. */

  /* Operation related flags */
  fre_op_f fre_op_flag;            /* Indicate the pattern's type of operation. */

  /* Back-reference related */
  bool     fre_op_bref;            /* True when back-reference(s) are found in a pattern. */
  fre_backref *backref_pos;        /* Contains positions of back-references, when fre_op_bref is true. */
  
  /* Pointer to the operation to be executed, determined by the fre_op_flag. */
  int      (*operation)(struct fpattern*, char*);

  /* Patterns */
  regex_t  *comp_pattern;          /* The compiled regex pattern. */
  char     *original_pattern;      /* The original, unmodified pattern. */  
  char     **striped_pattern;      /* Exactly 2 strings, holds patterns striped from Perl syntax elements. */

} fre_pattern;



/** Constants **/

# define FRE_MAX_PATTERN_LENGHT        256     /* 256 to keep our POSIX conformance. */
# define FRE_EXPECTED_M_OP_DELIMITER   2       /* Number of expected delimiters for a match op pattern. */
# define FRE_EXPECTED_ST_OP_DELIMITER  3       /* Number of expected delimiters for subs. and trans. op patterns. */
# define FRE_MAX_SUB_MATCHES           100     /* Maximum number of submatches. */
# define FRE_PMATCH_TABLE_SIZE         256     /* Arbitrary. */

/* Setting FRE_ARG_STRING_MAX_LENGHT to a bigger value than INT_MAX might result in overflow. */
# define FRE_ARG_STRING_MAX_LENGHT     INT_MAX /* Maximum lenght of fre_bind()'s string argument, '\0' included. */

# define FRE_OP_SUCCESSFUL             1       /* Indicate a successful operation. */
# define FRE_OP_UNSUCCESSFUL           0       /* Indicate an unsuccessful operation. */
# define FRE_ERROR                    -1       /* Indicate an error. */

static const char FRE_POSIX_DIGIT_RANGE[]     = "[0-9]";  /* Used mostly to replace '\d' escape sequence. */
static const char FRE_POSIX_NON_DIGIT_RANGE[] = "[^0-9]"; /* Used mostly to replace '\D' escape sequence. */


/* Simple error message. */
#define intern__fre__errmesg(string) do {		\
    if (errno) {					\
      perror(string);					\
    } else {						\
      fprintf(stderr, "%s ", string);			\
    }							\
    fprintf(stderr, "@ line: %d\n\n", __LINE__);	\
  } while (0);


/** Internal function prototypes **/

/* Library's init/finit. */
int intern__fre__lib_init(void);                  /* Initialize the library's globals. */
void intern__fre__lib_finit(void);                /* Free the library's globals memory. */


/* Memory allocation/deallocation routines. */
fre_backref* intern__fre__init_bref_arr(void);          /* Init an object to hold positions of back-refs inside a pattern. */
void intern__fre__free_bref_arr(fre_backref *to_free);  /* Free resources of a fre_backref * inited by _init_bref_arr(). */

void intern_line_test(void);
#endif /* FRE_INTERNAL_HEADER */
