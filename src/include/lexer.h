#ifndef LEXER
#define LEXER

#include "stack.h"
#include "token.h"

typedef struct {
  Token last_token;
  Stack* buffer;
  Stack* tokens;
  const char* filename;
  int linenum;
  int linedepth;
  char current_char;
} Lexer;

#endif
