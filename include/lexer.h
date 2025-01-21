#ifndef LEXER
#define LEXER

#include <vector>
#include <string>
#include <fstream>
#include <stack>
#include "token.h"
using namespace std;

class Lexer {
public:
  Token last_token;
  stack<Token> peeked_tokens;
  ifstream char_buffer;
  Token* current_token;

  string filename;
  int linenum;
  int linedepth;
  char current_char;

  Lexer(string filename);
  ~Lexer();
  Token* get_token();
  void unget_token();

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
};

#endif
