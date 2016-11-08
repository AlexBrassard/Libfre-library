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

