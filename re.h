#ifndef _TINY_REGEX_C
#define _TINY_REGEX_C



// REGULAR EXPRESSION SUPPORT
// Supports:
// ---------
//   '.'        Dot, matches any character
//   '^'        Start anchor, matches beginning of string
//   '$'        End anchor, matches end of string
//   '*'        Asterisk, match zero or more (greedy)
//   '+'        Plus, match one or more (greedy)
//   '?'        Question, match zero or one (non-greedy)
//   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
//   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently broken!
//   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
//   '\s'       Whitespace, \t \f \r \n \v and spaces
//   '\S'       Non-whitespace
//   '\w'       Alphanumeric, [a-zA-Z0-9_]
//   '\W'       Non-alphanumeric
//   '\d'       Digits, [0-9]
//   '\D'       Non-digits

#define RE_DOT_MATCHES_NEWLINE 1
typedef struct regex_t* re_t;
re_t re_compile(const char* pattern);
int  re_matchp(re_t pattern, const char* text, int* matchlength);
int  re_match(const char* pattern, const char* text, int* matchlength);



#endif
