/*
 *
 *  Libfre library  -  Internal MACROs.
 *  
 *
 */





/*
 * MACRO - Add current token to the given stack and
 * update the given stack's TOS.
 * Really isn't needed but it did make it easier on my brain when designing the library. 
 */
#define FRE_PUSH(token, stack, tos) do {	      \
    stack[*tos] = token;			      \
    ++(*tos);					      \
  } while (0) ;


/* Skip commentary tokens and adjust the pattern lenght afterward. */
#define FRE_SKIP_COMMENTS(pattern, new_pattern_len, token_ind)  do {	\
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
#define FRE_FETCH_MODIFIERS(pattern, freg_object, token_ind) do {      \
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
