/*
 *
 *  Libfre library  -  Internal header file.
 *
 */

/** Data structures. **/

/* Sub-match(es) begining/ending of match offsets structure. */
typedef struct fre_sm{
  int                   bo;               /* Offset of the begining of sub-match. */
  int                   eo;               /* Offset of the ending of sub-match. */

} fre_smatch;

/* Per-thread global sub-match table kept between invocations of fre_bind(). */
typedef struct fre_pmatch_tab {
  bool                  fre_modif_global; /* True when /g is activated, pmatch_table will contain extra nodes. */
  fre_smatch            *whole_match;     /* Begining/ending of the complete match operation in the matched string. */
  fre_smatch            **sub_match;      /* Array of sub-match offsets. */
  struct fre_pmatch_tab *next_match;      /* Pointer to the node containing positions of the next match. */
  
} fre_pmatch;

/* Flag to indicate a fre_pattern's operation. */
typedef enum foperation {
  NONE -1,
  MATCH 0,
  SUBSTITUTE,
  TRANSLITERATE
} fre_op_f;

/* Structure of a fre_pattern. */
typedef struct fpattern {

  bool     fre_mod_boleol;         /* True when the '/m' modifier is activated. */
  bool     fre_mod_newline;        /* True when the '/s' modifier is activated. */
  bool     fre_mod_icase;          /* True when the '/i' modifier is activated. */
  bool     fre_mod_ext;            /* True when the '/x' modifier is activated. */
  bool     fre_mod_global;         /* True when the '/g' modifier is activated. */
  bool     fre_p1_compiled;        /* True when stripped_pattern[0] has been regcomp()'d (to ease regfreeing) */
  bool     fre_paired_delimiters;  /* True when delimiter is one of " { ( [ < " */
  fre_op_f fre_op_flag;            /* Indicate the pattern's type of operation. */
  char     delimiter;              /* The delimiter used in the pattern. */
  char     c_delimiter;            /* Closing delimiter, when using paired-type delimiters. */
  /* Pointer to the operation to be executed, determined by the fre_op_flag. */
  int      (*operation)(struct fpattern*, char*);
  regex_t  *comp_pattern;          /* The compiled regex pattern. */
  char     **striped_pattern;      /* Exactly 2 strings, holds patterns striped from Perl syntax elements. */
  /* Make this local to _match_op() instead ? */
  int      **sub_match_pos;        /* Array of 9 arrays of int holding sub-matches positions. */
  
} fre_pattern;



/** Constants. **/
#define FRE_MAX_PATTERN_LENGHT        256 /* 256 to keep our POSIX conformance. */
#define FRE_EXPECTED_M_OP_DELIMITER   2   /* Number of expected delimiters for a match op pattern. */
#define FRE_EXPECTED_ST_OP_DELIMITER  3   /* Number of expected delimiters for substitute and transliterate op patterns. */
#define FRE_MAX_SUB_MATCHES           9   /* Maximum number of submatches. */

#define FRE_OP_SUCCESSFUL             1   /* Indicate a successful operation. */
#define FRE_OP_UNSUCCESSFUL           0   /* Indicate an unsuccessful operation. */
#define FRE_ERROR                    -1   /* Indicate an error. */

static const char FRE_POSIX_DIGIT_RANGE[]     = "[0-9]";  /* Used mostly to replace '\d' escape sequence. */
static const char FRE_POSIX_NON_DIGIT_RANGE[] = "[^0-9]"; /* Used mostly to replace '\D' escape sequence. */
