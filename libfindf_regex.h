/*
 *
 *  Libfindf library: Private FRE regex utilities.
 *
 */

#ifndef FINDF_PRIVATE_REGEX_HEADER
# define FINDF_PRIVATE_REGEX_HEADER 1


/* Libfindf-regex contanst. */
#define FRE_MATCH_EXPECT_DELIMITER 2  /* Number of expected delimiters in a match pattern. */
#define FRE_SUBST_EXPECT_DELIMITER 3  /* Number of expected delimiters in a substitution pattern. */
/*
 * Maximum number of sub-matches captured by libfindf (POSIX's default).
 * Libfindf internaly uses 1 of the 9 possibly available back-refs.
 * Every substitution operation has it's whole expression surrounded by a () backref operator.
 */
#define FRE_MAX_SUB_MATCHES 9
#define FRE_MATCH_UNSUCCESSFUL 2      /* Indicate an unsuccessful match, 0 == success (using RF_OPSUCC) . */

/* The next 2 constants are arrays since we copy them one char at a time. */
static const char FRE_DIGIT_RANGE[] = "[0-9]";
static const char FRE_NOT_DIGIT_RANGE[] = "[^0-9]";



typedef struct fstorage{
  char             *pattern1;
  char             *pattern2;

} findf_opstore_f;

/* Libfindf's regex data structure. */
typedef struct fregex{
  bool             fre_modif_boleol;         /* True when the '/m' modifier is activated. */
  bool             fre_modif_newline;        /* True when the '/s' modifier is activated. */
  bool             fre_modif_icase;          /* True when the '/i' modifier is activated. */
  bool             fre_modif_ext;            /* True when the '/x' modifier is activated. */
  bool             fre_modif_global;         /* True when the '/g' modifier is activated. */
  bool             fre_op_match;             /* True when operation is match. */
  bool             fre_op_substitute;        /* True when operation is substitute. */
  bool             fre_p1_compiled;          /* True when ->pattern[0] has been regcompiled.(to ease freeing) */
  bool             fre_paired_delimiter;     /* True when ->delimiter is one of '<' '(' '{' '[' . */
  char             delimiter;                /* The delimiter used by the pattern. */
  char             close_delimiter;          /* The closing delimiter, only used with paired delimiters. */
  int              (*operation)(struct fregex*, char *); /* A pointer to the operation to execute on the regex. */
  int              fre_op_return_val;        /* operation's return value. */
  regex_t          *pattern;                 /* A compiled regex pattern,  via a call to regcomp(). */
  findf_opstore_f  *pat_storage;             /* To hold stripped off, unmodified patterns. */
  int              **sub_match;              /* Array of 9 arrays of 2 elements [bom/eom]* to hold submatches positions. */

} findf_regex_f;
/* bom/eom: Begining Of sub-Match/Ending Of sub-Match. */





findf_regex_f* intern__findf__init_regex(void);
/* Release resources of a findf_regex_f object. */
int intern__findf__free_regex(findf_regex_f *to_free);
/* Release resources of an array of findf_regex_f object. */
int intern__findf__free_regarray(findf_regex_f **reg_array,
				 size_t numof_patterns);
/* Initialize the regex parser. */
findf_regex_f** intern__findf__init_parser(char **patterns,
					   size_t numof_patterns);
/* Parse a matching pattern. */
int intern__findf__strip_match(char *pattern,
			       size_t token_ind,
			       findf_regex_f *freg_object);
/* Parse a substitution or transliteration pattern. */
int intern__findf__strip_substitute(char *pattern,
				    size_t token_ind,
				    findf_regex_f *freg_object);
/* Check for unsupported escape sequences. */
int intern__findf__validate_esc_seq(char token,
				    char *buffer,
				    size_t *buf_ind,
				    size_t *buf_len);
/* Validate pattern's modifier(s). */
int intern__findf__validate_modif(char *modifiers,
				  findf_regex_f *freg_object);
/* Skip extended pattern's comments. */
void intern__findf__skip_comments(char *pattern,
				  size_t *cur_ind,
				  size_t *pattern_len);
/* Convert Perl-like syntax into POSIX ere syntax. */
int intern__findf__perl_to_posix(char *pattern,
				 findf_regex_f *freg_object);
/* Compile a POSIX regex pattern. */
int intern__findf__compile_pattern(findf_regex_f *freg_object);


/* Fre pattern operations related routines. */

/* Execute a pattern match operation. */
int intern__findf__match_op(struct fregex *freg_object,
			    char *filename);
/* Execute a substitution operation. */


#endif /* FINDF_PRIVATE_REGEX_HEADER */
