#pragma once

#include "lexer.h"
#include "node_manager.h"
#include "token.h"
#include "scope_manager.h"

#include <vector>
#include <string>

class SageParser {
public:
  SageLexer *lexer;
  Token *current_token;
  NodeIndex node_cache;
  vector<Token> errors;
  string filename;
  NodeManager *node_manager;
  ScopeManager *scope_manager;
  int symbol_count;

  SageParser();
  SageParser(ScopeManager *scope_manager, NodeManager *node_manager, string filename);
  ~SageParser();

  NodeIndex parse_program(bool debug_lexer);

private:
  // parsing methods
  NodeIndex parse_statements();
  NodeIndex parse_statement();
  NodeIndex parse_run_directive();
  NodeIndex parse_value_dec();
  NodeIndex parse_value_dec_list();
  NodeIndex parse_assign();
  NodeIndex parse_keyword_statement();
  NodeIndex parse_if_statement();
  NodeIndex parse_while_statement();
  NodeIndex parse_for_statement();
  NodeIndex parse_range();
  NodeIndex parse_construct();
  NodeIndex parse_struct();
  NodeIndex parse_function();
  NodeIndex parse_function_call();
  NodeIndex parse_body();
  NodeIndex parse_type();
  NodeIndex parse_expression();
  NodeIndex parse_operator(NodeIndex left, int min_precedence);
  NodeIndex parse_primary();
  NodeIndex parse_struct_field_access();

  // util methods
  bool match_types(TokenType type_a, TokenType type_b);
  bool matches_any(TokenType type_a, TokenType *possible_types, int type_amount);
  void consume(TokenType expected_type, string message);
  void advance();
  Token peek();
  bool op_is_left_associative(string op_literal);
  bool op_is_right_associative(string op_literal);

};
