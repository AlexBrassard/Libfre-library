/*
 *
 *  Libfre library  -  Internal MACROs.
 *  
 *
 */





/*
 * Add current token to the given stack and
 * update the given stack's TOS.
 * Really isn't needed but it did make it easier on my brain when designing the library. 
 */
#ifndef FRE_INTERNAL_MACRO_HEADER
# define FRE_INTERNAL_MACRO_HEADER 1

# define FRE_PUSH(token, stack, tos) do {	      \
    stack[*tos] = token;			      \
    ++(*tos);					      \
  } while (0) ;


/* Skip commentary tokens and adjust the pattern lenght afterward. */
# define FRE_SKIP_COMMENTS(pattern, new_pattern_len, token_ind)  do {	\
    size_t i = 0;							\
    while(pattern[*token_ind] != '\0') {				\
      if (pattern[*token_ind] == '\\' && pattern[(*token_ind) + 1] == 'n'){ \
	(*token_ind) += 2;						\
	break;								\
      }									\
      ++(*token_ind);							\
      ++i;								\
    }									\
    /* Adjust the pattern's lenght and check for overflow. */		\
    if (((*new_pattern_len) - i > 0)					\
	&& ((*new_pattern_len) - i < FRE_MAX_PATTERN_LENGHT)) {		\
      (*new_pattern_len) -= i;						\
    } else {								\
      errno = 0;                                                        \
      intern__fre__errmesg("FRE_SKIP_COMMENTS failed to adjust pattern's lenght"); \
      errno = EOVERFLOW;                                                \
    }                                                                   \
  }while (0);


/* Fetch a user's pattern modifier(s). */
# define FRE_FETCH_MODIFIERS(pattern, freg_object, token_ind) do {      \
    while (pattern[*token_ind] != '\0') {			       \
      switch (pattern[*token_ind]) {				       \
      case 'g':							       \
	freg_object->fre_mod_global = true;			       \
	break;							       \
      case 'i':							       \
	freg_object->fre_mod_icase = true;			       \
	break;							       \
      case 's':							       \
	freg_object->fre_mod_newline = true;			       \
	break;							       \
      case 'm':							       \
	freg_object->fre_mod_boleol = true;			       \
	break;							       \
      case 'x':							       \
	freg_object->fre_mod_ext = true;			       \
	break;							       \
      default:							       \
	errno = 0;						       \
	intern__fre__errmesg("Unknown modifier in pattern.");	       \
	return FRE_ERROR;					       \
      }								       \
      (*token_ind)++;						       \
    }								       \
    return FRE_OP_SUCCESSFUL;					       \
  } while(0) ;


/*
 * Registers a backreference's position within the pattern it's been found
 * and also registers its real backreference number (the one found after the '\' or '$'). 
 */
# define FRE_HANDLE_BREF(pattern, token_ind, sub_match_ind, fre_is_sub, freg_object)do { \
    char refnum_string[FRE_MAX_PATTERN_LENGHT];				\
    long refnum = 0;							\
    size_t i = 0;							\
    if (!fre_is_sub) {							\
      freg_object->backref_pos->in_pattern[freg_object->backref_pos->in_pattern_c] = sub_match_ind; \
      freg_object->fre_match_op_bref = true;				\
    }									\
    else {								\
      freg_object->backref_pos->in_substitute[freg_object->backref_pos->in_substitute_c] = sub_match_ind; \
      freg_object->fre_subs_op_bref = true;				\
    }									\
    memset(refnum_string, 0, FRE_MAX_PATTERN_LENGHT);			\
    ++(*token_ind);							\
    do {								\
      refnum_string[i++] = pattern[(*token_ind)++];			\
    } while (isdigit(*token_ind));					\
    refnum = atol(refnum_string);					\
    if (!fre_is_sub){							\
      freg_object->backref_pos->p_sm_number[freg_object->backref_pos->in_pattern_c++] = refnum; \
    }									\
    else{								\
      freg_object->backref_pos->s_sm_number[freg_object->backref_pos->in_substitute_c++] = refnum; \
    }									\
  } while(0);


/*
 * Checks each characters inside the \Q..\E|\0 to see if it's a metacharacter.
 * When encountering a POSIX metacharacter, checks to see if the new pattern has
 * enough free memory to hold an extra backslash and push a backslash before pushing the
 * actual metacharacter to the pattern's stack.
 * Takes the fre_pattern carried throughout the library, the token of the backslash
 * begining the \Q... sequence, the pattern being built is qfreg_object->striped_pattern[0]
 * (freg_object->striped_pattern[1] isn't allowed to reach that far in perl_to_posix)
 * the new pattern's top of stack and lenght.
 * Make careful use of this MACRO since it may returns an integer. 
 */
# define FRE_QUOTE_TOKENS(qfreg_object, qpattern, qtoken_ind, qnew_pat_tos, qnew_pat_len) do { \
    size_t new_pat_len = qnew_pat_len;                                  \
    (*qtoken_ind) += 2; /* To be at the first token of the sequence */  \
    while(qpattern[*qtoken_ind] != '\0'){                               \
      if (qpattern[*qtoken_ind] == '\\'){                               \
	if (qpattern[(*qtoken_ind)+1] == 'E') {                         \
	  (*qtoken_ind) += 2;                                           \
	  break;                                                        \
	}                                                               \
      }                                                                 \
      if ((qpattern[*qtoken_ind] == qfreg_object->delimiter && qfreg_object->fre_paired_delimiters == false) \
	  || (qpattern[*qtoken_ind] == qfreg_object->c_delimiter && qfreg_object->fre_paired_delimiters == true)) break; \
      switch(qpattern[*qtoken_ind]) {                                   \
      case '.' : case '[' : case '\\':                                  \
      case '(' : case ')' : case '*' :                                  \
      case '+' : case '?' : case '{' :                                  \
      case '|' : case '^' : case '$' :                                  \
	if ((new_pat_len += 1) >= FRE_MAX_PATTERN_LENGHT){              \
	  errno = 0; intern__fre__errmesg("Overflow replacing Perl-like \\Q...\\E escape sequence"); \
	  errno = EOVERFLOW;                                            \
	  return FRE_ERROR;                                             \
	}                                                               \
	FRE_PUSH('\\', qfreg_object->striped_pattern[0], qnew_pat_tos); \
	break;                                                          \
      }                                                                 \
      FRE_PUSH(qpattern[*qtoken_ind], qfreg_object->striped_pattern[0], qnew_pat_tos); \
      (*qtoken_ind) += 1;                                               \
    }                                                                   \
  } while (0);


/*
 * Replaces a word boundary token sequence if we can, else raise
 * the appropriate fre_pattern flag.
 * Takes the fre_pattern, the ->striped_pattern's top of stack, the new pattern _p_t_p() is builing, its top of stack
 * and lenght, a 0 or 1 value indicating whether this is the matching or substitute pattern and a 0 or 1                   
 * value indicating whether it's a word boundary sequence: '\<' '\>' '\b' or a not a word boundary sequence '\B'.
 * This MACRO might return an integer, careful about using it elsewhere than _perl_to_posix(). 
 */
# define FRE_REGISTER_BOUNDARY(freg_objet, spa_tos, new_pat, new_pat_tos, new_pat_len, is_sub, is_word) do{ \
    size_t i = 0;                                                       \
    if (spa_tos == 0){                                                  \
      if (!is_sub) freg_object->fre_match_op_bow = true;                \
      else freg_object->fre_subs_op_bow = true;                         \
    } else if (spa_tos == (strlen(freg_object->striped_pattern[is_sub])-2)){ \
      if (!is_sub) freg_object->fre_match_op_eow = true;                \
      else freg_object->fre_subs_op_eow = true;                         \
    }                                                                   \
    else {                                                              \
      if (is_word){                                                     \
	if ((*new_pat_len += strlen(FRE_POSIX_NON_WORD_CHAR)+1) >= FRE_MAX_PATTERN_LENGHT){ \
	  errno = 0; intern__fre__errmesg("Overflow replacing a word boundary sequence"); \
	  errno = EOVERFLOW; return FRE_ERROR;                          \
	}                                                               \
	while(FRE_POSIX_NON_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_WORD_CHAR[i++], new_pat, new_pat_tos); \
      } else {                                                          \
	if ((*new_pat_len += strlen(FRE_POSIX_WORD_CHAR)+1) >= FRE_MAX_PATTERN_LENGHT){ \
	  errno = 0; intern__fre__errmesg("Overflow replacing a word boundary sequence"); \
	  errno = EOVERFLOW; return FRE_ERROR;                          \
	}                                                               \
	while (FRE_POSIX_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_WORD_CHAR[i++], new_pat, new_pat_tos); \
      }                                                                 \
    }                                                                   \
  } while (0);


/*
 * This MACRO is used to verify each escape sequence found in the user's given pattern,
 * and replace any sequences not supported by the POSIX standard.
 * Takes the parsed pattern, the token index of the backslash leading the escape sequence,
 * the new pattern made by _perl_to_posix, the new pattern's top of stack, the new pattern's
 * current lenght and the fre_pattern object used throughout the library.
 * This MACRO returns an integer, careful about using it elsewhere than _perl_to_posix()! 
 */
# define FRE_CERTIFY_ESC_SEQ(is_sub, token_ind, new_pat, new_pat_tos, new_pat_len, freg_object) do { \
    size_t i = 0;                                                       \
    switch(freg_object->striped_pattern[is_sub][*token_ind + 1]){       \
    case 'd':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Pattern lenght exceed maximum converting Perl-like digit range '\\d'"); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while (FRE_POSIX_DIGIT_RANGE[i] != '\0') FRE_PUSH(FRE_POSIX_DIGIT_RANGE[i++], new_pat, new_pat_tos); \
      /* Adjust the token index. */                                     \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 'D':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_NON_DIGIT_RANGE)) >= FRE_MAX_PATTERN_LENGHT) { \
	errno = 0; intern__fre__errmesg("Pattern lenght exceed maximum converting Perl-like not digit range '\\D'"); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while(FRE_POSIX_NON_DIGIT_RANGE[i] != '\0') FRE_PUSH(FRE_POSIX_NON_DIGIT_RANGE[i++], new_pat, new_pat_tos); \
      /* Adjust the token index. */                                     \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 'w':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_WORD_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow convering Perl-like '\\w' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while (FRE_POSIX_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_WORD_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;                                                  \
      break;                                                            \
      /* More test will go here. */                                     \
    case 'W':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_NON_WORD_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\W' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while (FRE_POSIX_NON_WORD_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_WORD_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 's':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_SPACE_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\s' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while (FRE_POSIX_SPACE_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_SPACE_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 'S':                                                           \
      if ((*new_pat_len += strlen(FRE_POSIX_NON_SPACE_CHAR)) >= FRE_MAX_PATTERN_LENGHT){ \
	errno = 0; intern__fre__errmesg("Overflow converting Perl-like '\\S' into a POSIX construct."); \
	errno = EOVERFLOW; return FRE_ERROR;                            \
      }                                                                 \
      while (FRE_POSIX_NON_SPACE_CHAR[i] != '\0') FRE_PUSH(FRE_POSIX_NON_SPACE_CHAR[i++], new_pat, new_pat_tos); \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 'b': /* Fall throught */                                       \
    case '<': /* Fall throught */                                       \
    case '>':                                                           \
      FRE_REGISTER_BOUNDARY(freg_object, *token_ind, new_pat,           \
			    new_pat_tos, new_pat_len, is_sub, 1);       \
      *token_ind += 2;                                                  \
      break;                                                            \
    case 'B':                                                           \
      FRE_REGISTER_BOUNDARY(freg_object, *token_ind, new_pat,           \
			    new_pat_tos, new_pat_len, is_sub, 0);       \
      freg_object->fre_not_boundary = true;                             \
      *token_ind += 2;                                                  \
      break;                                                            \
    default:                                                            \
      /* Assume a supported sequence, push the tokens on the pattern stack. */ \
      FRE_PUSH(freg_object->striped_pattern[is_sub][(*token_ind)++], new_pat, new_pat_tos); \
      FRE_PUSH(freg_object->striped_pattern[is_sub][(*token_ind)++], new_pat, new_pat_tos); \
      /*(*token_ind) += 2;*/                                            \
      break;                                                            \
    }                                                                   \
  }while (0);


/* ASCII only! 

Most likely obsolete.

*/
# define FRE_INSERT_DIGIT_RANGE(array, token_ind) do {			\
    size_t i = 1; /* The zeroth char of FRE_POSIX_DIGIT_RANGE is '[' */ \
    while (FRE_POSIX_DIGIT_RANGE[i] != ']') {                           \
      array[(*token_ind)++] = FRE_POSIX_DIGIT_RANGE[i++];               \
    }                                                                   \
  } while (0);

/*
 * Verify and insert a dash separated range.
 * A dash separated range may be between any two printable characters,
 * so long as they appear in increasing order. It depends greatly on your locale.
 * Starting at *token_ind, insert the appropriate range in array.
 * ASCII only!!! 
 */
# define FRE_INSERT_DASH_RANGE(array, token_ind, low, high) do { \
    int temp = 0;                                               \
    int current = low;                                          \
    if (low >= high){                                           \
      temp = low; low = high; high = temp;                      \
    }                                                           \
    while (current <= high)                                     \
      array[(*token_ind)++] = current++;                        \
  } while (0);


/*
 * Takes the fre_pattern we're working on, the caller's string, a 0 or 1 value indicating whether
 * to check on either of the matching and substitute pattern respectively, the number of tokens
 * skipped by previous _cut_match() operations and a pointer to an int
 * indicating whether we had success or not. 1 all good, -1 error, 0 not a boundary.
 
 * Note that in cases when a not-a-word boundary \B is used, *ret must be set to 
 * 0 if it's 1 and 1 if it's 0 once the checks are made.
 */

# define FRE_CHECK_BOUNDARY(freg_object, string, numof_tokens, is_sub, ret) do { \
    bool op_bow = ((is_sub) ? freg_object->fre_subs_op_bow : freg_object->fre_match_op_bow); \
    bool op_eow = ((is_sub) ? freg_object->fre_subs_op_eow : freg_object->fre_match_op_eow); \
    if (op_bow == true){                                                \
      if (fre_pmatch_table->whole_match[WM_IND]->bo > 0){               \
	if (isalpha(string[fre_pmatch_table->whole_match[WM_IND]->bo-(*numof_tokens)-1])) *ret = 0; \
	else *ret = 1;                                                  \
      }                                                                 \
      else { *ret = 1;                                                  \
      }                                                                 \
      /* Inverse resuls of previous op if the boundary sequence is '\B' */ \
      if (freg_object->fre_not_boundary == true){                       \
	if (*ret) *ret = 0; else *ret = 1;                              \
      }                                                                 \
    }                                                                   \
    if (op_eow == true){                                                \
      /* String's lenght has been checked to be under INT_MAX-1 when entering the library. */ \
      if (fre_pmatch_table->whole_match[WM_IND]->eo < (int)strlen(string)-1){ \
	if (isalpha(string[fre_pmatch_table->whole_match[WM_IND]->eo-(*numof_tokens)])) *ret = 0; \
	else *ret = 1;                                                  \
      }                                                                 \
      else { *ret = 1;                                                  \
      }                                                                 \
      /* Inverse resuls of previous op if the boundary sequence is '\B' */ \
      if (freg_object->fre_not_boundary == true){                       \
	if (*ret) *ret = 0; else *ret = 1;                              \
      }                                                                 \
    }                                                                   \
  } while(0);


/*
 * Reset the ptable's ->whole_match[WM_IND] field and all it's corresponding
 * sub-matches to -1.
 */
# define FRE_CANCEL_CUR_MATCH() do {			      \
    int i = 0;						      \
    fre_pmatch_table->whole_match[WM_IND]->bo = -1;	      \
    fre_pmatch_table->whole_match[WM_IND]->eo = -1;	      \
    while(i++ < fre_pmatch_table->subm_per_match){	      \
      fre_pmatch_table->sub_match[SM_IND]->bo = -1;	      \
      fre_pmatch_table->sub_match[SM_IND]->eo = -1;	      \
      if (SM_IND > 0) --SM_IND;				      \
    }							      \
  } while(0);

# define FRE_CANCEL_ALL_MATCH() do {                             \
    while(1) {                                                  \
      FRE_CANCEL_CUR_MATCH();                                   \
      if (WM_IND > 0) --WM_IND;                                 \
      else break;                                               \
    }								\
  } while(0);


#endif /* FRE_INTERNAL_MACRO_HEADER */
