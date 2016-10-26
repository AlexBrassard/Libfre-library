/*
 *
 *  Libfre library  -  Internal header file.
 *  Version   0.600
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

/* To serialize messages containing multiple function calls. */
extern pthread_mutex_t fre_stderr_mutex; /* Defined and initialized in "fre_internal_init.c" */


/** Data structures **/


/* Sub-match(es) begining/ending of match offsets structure. */
typedef struct fre_sm{
  int                   bo;               /* Offset of the begining of sub-match. */
  int                   eo;               /* Offset of the ending of sub-match. */

} fre_smatch;


/* To keep track of where in each patterns are the back-references. */
typedef struct fre_brefs {
  int                   *in_pattern;    /* 
					 * [0] is relative to the begining of stripped_pattern[0],
					 * [1+] are relative to the begining of their previous element.
					 * (3 rel 2, 2 rel 1, 1 rel 0).
					 */
  long                  *p_sm_number;    /*
					  * Used with ->in_pattern.
					  * in_pattern's sub-match numbers (to catch cases where the first 
					  * backref would not be the first sub-match.)
					  */
  int                   *in_substitute;  /*
					  * [0] is relative to the begining of stripped_pattern[1],
					  * [1+] are relative to the begining of their previous element.
					  */
  long                  *s_sm_number;    /* Used with ->in_substitute. in_substitute's sub-match numbers. */
  size_t                in_pattern_c;    /* First free empty element of in_pattern. */
  size_t                in_substitute_c; /* First free empty element of in_substitute. */

} fre_backref;


/* Keep track of word boundaries pattern positions. */
typedef struct fbwpos {
  bool                  fre_word_bound;  /* True when pattern contains word boundaries. */
  bool                  fre_nword_bound; /* True when patter contains non-word boundaries. */
  int                   *boundary_pos;   /* Positions of word boundaries within the regex pattern. */
  int                   *nboundary_pos;  /* Position of non-word boundaries within the regex pattern. */
  int                   bp_count;        /* First free position of boundary_pos. */
  int                   nbp_count;       /* First free position of nboundary_pos. */
  int                   bp_size;         /* Size of boundary_pos. */
  int                   nbp_size;        /* Size of nboundary_pos. */

} fre_bound_pos;

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
  bool                  fre_mod_boleol;         /* True when the '/m' modifier is activated. */
  bool                  fre_mod_newline;        /* True when the '/s' modifier is activated. */
  bool                  fre_mod_icase;          /* True when the '/i' modifier is activated. */
  bool                  fre_mod_ext;            /* True when the '/x' modifier is activated. */
  bool                  fre_mod_global;         /* True when the '/g' modifier is activated. */
  bool                  fre_mod_sub_is_regex;   /* True when treating substitute pattern like a regex pattern. */

  /* Indicate whether or not to regfree() the pattern. */
  bool                  fre_p1_compiled;        /* True when stripped_pattern[0] has been regcomp()'d. */

  /* Pattern's delimiter(s) */
  bool                  fre_paired_delimiters;  /* True when delimiter is one of  { ( [ <  */
  char                  delimiter;              /* The delimiter used in the pattern. */
  char                  c_delimiter;            /* Closing delimiter, when using paired-type delimiters. */

  /* Operation related flags */
  fre_op_f              fre_op_flag;            /* Indicate the pattern's type of operation. */

  /* Back-reference related */
  bool                  fre_match_op_bref;      /* True when back-reference(s) are found in a matching pattern. */
  bool                  fre_subs_op_bref;       /* True when back-reference(s) are found in the substitute pattern. */
  fre_backref           *backref_pos;           /* Contains positions of back-references, when fre_op_bref is true. */
  
  /* Word boundaries, pattern positions. */
  fre_bound_pos         *bound_pos;             /* Holds positions in pattern of word boundaries, if any. */

  /* Patterns */
  regex_t               *comp_pattern;         /* The compiled regex pattern. */
  char                  **striped_pattern;     /* Exactly 2 strings, holds patterns striped from Perl syntax elements. */
  char                  **saved_pattern;       /* Exactly 2 strings, copies of strip_pattern[0|1] before _atch_op modifies them. */

} fre_pattern;


/* Per-thread global sub-match table kept between invocations of fre_bind(). */
typedef struct fre_pmatch_tab {
  bool                  fre_saved_object;      /* True when an fre_pattern* has been saved in ->ls_object. */
  int                   lastop_retval;         /* To keep return value of a successful match_op in global operations. */
  char                  *ls_pattern;           /* Last seen pattern, as given by the caller of fre_bind(). */
  fre_pattern           *ls_object;            /* Last seen freg_object, as returned by the _plp_parser(). */
  fre_smatch            **whole_match;         /* Begining/ending positions of every successful matches. */
  fre_smatch            **sub_match;           /* Begining/ending positions of every parenthesed expressions. */
  int                   subm_per_match;        /* Number of submatches per matches. */
  size_t                wm_ind;                /* First free position of whole-match array. */
  size_t                sm_ind;                /* First free position of sub-matches array. */
  size_t                wm_size;               /* Size of whole-match array. */
  size_t                sm_size;               /* Size of sub-match array. */
  
} fre_pmatch;


/* 
 * Global table containing pointers to headnodes of all created 
 * pmatch-tables to allow _lib_finit to free them.
 */
typedef struct fre_head_tab {
  size_t                numof_tables;          /*obsolete Number of nodes in the array. */
  size_t                sizeof_table;          /* The size of the array. */
  size_t                head_list_tos;         /* The headnode list's first available element. */
  pthread_mutex_t       *table_lock;           /* To serialize access to the table. */
  fre_pmatch            **head_list;           /* Array of pointers to pmatch_table headnodes. */


} fre_headnodes;




/** Constants **/

# define FRE_MAX_PATTERN_LENGHT        256     /* 256 to keep our POSIX conformance. */
# define FRE_EXPECTED_M_OP_DELIMITER   2       /* Number of expected delimiters for a match op pattern. */
# define FRE_EXPECTED_ST_OP_DELIMITER  3       /* Number of expected delimiters for subs. and trans. op patterns. */
# define FRE_MAX_MATCHES               128     /* Default maximum number of matches. */
# define FRE_MAX_SUB_MATCHES           32      /* Default maximum number of submatches. */
# define FRE_HEADNODE_TABLE_SIZE       4       /* Arbitrary. Default number of pmatch_tables in the headnode_table. */
# define FRE_MAX_WORD_BOUNDARIES       64      /* Arbitrary. Maximum number of recorded word/non-word boundaries per pattern. */

/* Must be INT_MAX to safely fetch sub-match(es) position(s). */
# define FRE_ARG_STRING_MAX_LENGHT     INT_MAX /* Maximum lenght of fre_bind()'s string argument, '\0' included. */

/* 
 * Successful is set to 1, it makes for prettier 'if' statements. 
 * Ex: if (fre_bind(pattern,string)) { puts("Matched!"); }
 */
# define FRE_OP_SUCCESSFUL             1       /* Indicate a successful operation. */
# define FRE_OP_UNSUCCESSFUL           0       /* Indicate an unsuccessful operation. */
# define FRE_ERROR                    -1       /* Indicate an error. */

static const char FRE_PAIRED_O_DELIMITERS[]   = "<({[";          /* Opening paired-type delimiters. */
static const char FRE_PAIRED_C_DELIMITERS[]   = ">)}]";          /* Closing paired-type delimiters. */
static const char FRE_POSIX_DIGIT_RANGE[]     = "[[:digit:]]";   /* Used to replace '\d' escape sequence. */
static const char FRE_POSIX_NON_DIGIT_RANGE[] = "[^[:digit:]]";  /* Used to replace '\D' escape sequence. */
static const char FRE_POSIX_WORD_CHAR[]       = "[_[:alpha:]]";  /* Used to replace '\w' escape sequence. */
static const char FRE_POSIX_NON_WORD_CHAR[]   = "[^_[:alpha:]]"; /* Used to replace '\W' escape sequence. */
static const char FRE_POSIX_SPACE_CHAR[]      = "[[:space:]]";   /* Used to replace '\s' escape sequence. */
static const char FRE_POSIX_NON_SPACE_CHAR[]  = "[^[:space:]]";  /* Used to replace '\S' escape sequence. */
extern fre_headnodes *fre_headnode_table;                 /* Global table of linked-lists headnodes, use with care. */

/* Simple error message. */
# define intern__fre__errmesg(string) do {		\
    pthread_mutex_lock(&fre_stderr_mutex);		\
    if (errno) {					\
      perror(string);					\
    } else {						\
      fprintf(stderr, "%s ", string);			\
    }							\
    fprintf(stderr, "@ line: %d\n\n", __LINE__);	\
    pthread_mutex_unlock(&fre_stderr_mutex);		\
  } while (0);




/*** Internal function prototypes ***/

void *SU_strcpy(char *dest, char *src, size_t n);                  /* Safely copy src to dest[n] */

/** Library's init/finit. **/
int          intern__fre__lib_init(void);                          /* Initialize the library's globals. */
void         intern__fre__lib_finit(void);                         /* Free the library's globals memory. */


/** Memory allocation/deallocation routines. **/

fre_backref*   intern__fre__init_bref_arr(void);                     /* Allocate memory to a fre_backref* object. */
void           intern__fre__free_bref_arr(fre_backref *to_free);     /* Free resources of a fre_backref *. */
/* There's no _free_smatch(), _free_pmatch_node handles this. */
fre_smatch*    intern__fre__init_smatch(void);                       /* Allocate memory to a fre_smatch object. */
fre_pmatch*    intern__fre__init_pmatch_table(void);
void           intern__fre__free_pmatch_table(fre_pmatch *node);
fre_bound_pos* intern__fre__init_bound_pos(void);                    /* Initialize a fre_bound_pos object. */
void           intern__fre__free_bound_pos(fre_bound_pos *to_free);  /* Release resources of a single fre_bound_pos object. */ 
fre_pattern*   intern__fre__init_pattern(void);                      /* Initialize a fre_pattern object. */
void           intern__fre__free_pattern(fre_pattern *freg_object);  /* Release resources of a fre_pattern object */
fre_headnodes* intern__fre__init_head_table(void);                   /* Init the global table of headnode pointers. */
void           intern__fre__free_head_table(void);                   /* Release resources of the global headnode_table. */
int            intern__fre__push_head(fre_pmatch* head);             /* Add the given headnode to the global headnode_table. */
void           intern__fre__key_delete(void *key);                   /* To feed pthread_key_create() . */
fre_pmatch*    intern__fre__pmatch_location(void);                   /* To access a thread's pmatch-table. */
void           intern__fre__clean_head_table(void);                  /* Free memory used by all pmatch-tables created. */

int            intern__fre__compile_pattern(fre_pattern *freg_object);/* Compile the modified pattern. */
int            intern__fre__insert_sm(fre_pattern *freg_object,       /* Insert all sub-matches in the given pattern. */
				      char *string,
				      int numof_tokens,
				      size_t is_sub);
char*          intern__fre__cut_match(char *string,                   /* Remove a character sequence from a string. */
				      size_t *numof_tokens_skiped,
				      size_t string_size,
				      size_t bo,
				      size_t eo);

/* DEBUG only. */
void print_pattern_hook(fre_pattern* pat);
void print_ptable_hook(void);

/* Likely to become MACROS. */
void FRE_INSERT_DASH_RANGE(char *array,
			   size_t *token_ind,
			   int low,
			   int high);
int FRE_HANDLE_BREF(char *pattern,
		    size_t *token_ind,
		    size_t sub_match_ind,
		    int fre_is_sub,
		    fre_pattern *freg_object);
int FRE_FETCH_MODIFIERS(char *pattern,
			fre_pattern *freg_object,
			size_t *token_ind);
int FRE_SKIP_COMMENTS(char *pattern,
		      size_t *pattern_len,
		      size_t *token_ind);
/**/

/** Regex Parser utility routines. **/

fre_pattern* intern__fre__plp_parser(char *pattern);               /* The Perl-like Pattern Parser. */
int          intern__fre__split_pattern(char * pattern,            /* Strip a pattern from its Perl-like elements. */
					fre_pattern *freg_object,
					size_t token_ind);
int          intern__fre__perl_to_posix(fre_pattern *freg_object, /* Convert Perl-like constructs into POSIX constructs. */
					size_t is_sub);
					
/** Regex operations routines. **/
int          intern__fre__match_op(char *string,                   /* Execute a match operation. */			  
				   fre_pattern *freg_object,
				   size_t *offset_to_start);
int          intern__fre__substitute_op(char *string,              /* Execute a substitution operation. */
					size_t string_size,
					fre_pattern *freg_object,
				        size_t *offset_to_start);
int          intern__fre__transliterate_op(char *string,           /* Execute a transliteration operation. */
					   size_t string_size,
					   fre_pattern *freg_object);

/* Thread specific pmatch-table. */
#define fre_pmatch_table (intern__fre__pmatch_location())


#endif /* FRE_INTERNAL_HEADER */
