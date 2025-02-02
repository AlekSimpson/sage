#ifndef PARSER
#define PARSER

#include "lexer.h"
#include "parse_node.h"
#include "token.h"

#include <vector>
#include <string>

class Parser {
public:
  Lexer* lexer;
  Token* current_token;
  AbstractParseNode* node_cache;
  vector<Token> errors;

  Parser(string filename);

  AbstractParseNode* parse_program(bool debug_lexer);

private:
  // parsing methods
  AbstractParseNode* library_statement();
  BlockParseNode* parse_libraries();
  BlockParseNode* parse_statements();
  AbstractParseNode* parse_statement();
  AbstractParseNode* parse_run_directive();
  AbstractParseNode* parse_value_dec();
  AbstractParseNode* parse_value_dec_list();
  AbstractParseNode* parse_assign();
  AbstractParseNode* parse_keyword_statement();
  AbstractParseNode* parse_if_statement();
  AbstractParseNode* parse_while_statement();
  AbstractParseNode* parse_for_statement();
  AbstractParseNode* parse_range();
  AbstractParseNode* parse_construct();
  AbstractParseNode* parse_struct();
  AbstractParseNode* parse_function();
  AbstractParseNode* parse_function_call();
  AbstractParseNode* parse_body();
  AbstractParseNode* parse_type();
  AbstractParseNode* parse_expression();
  AbstractParseNode* parse_operator(AbstractParseNode* left, int min_precedence);
  AbstractParseNode* parse_primary();
  UnaryParseNode* parse_struct_field_access();

  // util methods
  Token raise_error(string message);
  bool match_types(TokenType type_a, TokenType type_b);
  bool matches_any(TokenType type_a, TokenType* possible_types, int type_amount);
  Token consume(TokenType expected_type, string message);
  void advance();
  Token peek();
  bool current_token_type_is(TokenType token_type);
  bool is_ending_token();
  bool op_is_left_associative(string op_literal);
  bool op_is_right_associative(string op_literal);
};

#endif
