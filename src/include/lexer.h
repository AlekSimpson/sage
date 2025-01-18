#ifndef LEXER
#define LEXER

#include <vector>
#include <string>
#include "token.h"
#include "stack.h"

class Lexer {
public:
  /*Token last_token; // NOTE: Probably could just make this a function that returns the first element in the tokens buffer, but not sure yet*/
  stack<Token> tokens;
  stack<char> char_buffer;
  Token* current_token;

  string filename;
  int linenum;
  int linedepth;
  char current_char;

  Lexer(string filename, vector<char> buffer);
  ~Lexer();
  Token* get_token();
  void unget_token(Token* tok);

private:
  Token* lexer_make_token(TokenType type, string lexeme);
  Token* check_for_string();
  Token* handle_symbol_case(
      char default_char, TokenType default_type, 
      TokenType target_type, string target_symbol
  );
  Token* lex_for_symbols();
  Token* followed_by(char, TokenType, string);
  Token* lex_for_numbers();
  Token* lex_for_identifiers();
}

#endif
