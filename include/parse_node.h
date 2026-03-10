#pragma once

#include <string>
#include <vector>
#include "token.h"
using namespace std;

class NodeManager;

enum ParseNodeType {
  PN_BINARY = 0,
  PN_TRINARY,
  PN_UNARY,
  PN_NUMBER,
  PN_FLOAT,
  PN_STRING,
  PN_IDENTIFIER,//
  PN_KEYWORD,
  PN_BLOCK,
  PN_PARAM_LIST,
  PN_FUNCDEF,
  PN_FUNCCALL,
  PN_TYPE,
  PN_STRUCT,
  PN_IF,
  PN_IF_BRANCH,
  PN_ELSE_BRANCH,
  PN_WHILE,
  PN_ASSIGN,
  PN_FOR,
  PN_RANGE,
  PN_VAR_DEC,
  PN_VAR_REF,
  PN_RUN_DIRECTIVE,
  PN_LIST,
  PN_INCLUDE,
  PN_CHARACTER_LITERAL,
  PN_BOOL,
  PN_FIELD_ACCESS,
  PN_POINTER_DEREFERENCE,
  PN_POINTER_REFERENCE,
  PN_NOT,
  PN_STATIC_ARRAY_TYPE,
  PN_DYNAMIC_ARRAY_TYPE,
  PN_POINTER_TYPE,
  PN_ARRAY_REFERENCE_TYPE
};

string nodetype_to_string(ParseNodeType nodetype);

class AbstractParseNode {
public:
  virtual ~AbstractParseNode() {}
  virtual string to_string() = 0;
  virtual void showtree(string depth) = 0;
  virtual NodeManager *get_node_manager() = 0;
  virtual Token get_token() = 0;
  virtual ParseNodeType get_rep_nodetype() = 0;
  virtual ParseNodeType get_host_nodetype() = 0;
  
  // Scope and symbol binding for multi-phase compilation
  virtual int get_scope_id() = 0;
  virtual void set_scope_id(int id) = 0;
  virtual int get_resolved_symbol() = 0;
  virtual void set_resolved_symbol(int symbol_idx) = 0;
};

class BlockParseNode : public AbstractParseNode {
public:
  NodeManager *node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  int scope_id = -1;           // Scope this node belongs to
  int resolved_symbol = -1;    // Pre-resolved symbol index (-1 = unresolved)

  vector<int> children;

  BlockParseNode(NodeManager *man);
  BlockParseNode(NodeManager *man, Token token, ParseNodeType represents);
  BlockParseNode(NodeManager *man, Token token, ParseNodeType represents, vector<int> children);

  string to_string() override;
  NodeManager *get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
  int get_scope_id() override { return scope_id; }
  void set_scope_id(int id) override { scope_id = id; }
  int get_resolved_symbol() override { return resolved_symbol; }
  void set_resolved_symbol(int symbol_idx) override { resolved_symbol = symbol_idx; }
};

class BinaryParseNode : public AbstractParseNode {
public:
  NodeManager *node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  int scope_id = -1;           // Scope this node belongs to
  int resolved_symbol = -1;    // Pre-resolved symbol index (-1 = unresolved)

  int left;
  int right;

  BinaryParseNode(NodeManager *man, Token token, ParseNodeType represents, int left, int right);

  string to_string() override;
  NodeManager *get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
  int get_scope_id() override { return scope_id; }
  void set_scope_id(int id) override { scope_id = id; }
  int get_resolved_symbol() override { return resolved_symbol; }
  void set_resolved_symbol(int symbol_idx) override { resolved_symbol = symbol_idx; }
};

class TrinaryParseNode : public AbstractParseNode {
public:
  NodeManager *node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  int scope_id = -1;           // Scope this node belongs to
  int resolved_symbol = -1;    // Pre-resolved symbol index (-1 = unresolved)

  int left;
  int middle;
  int right;

  TrinaryParseNode(NodeManager *man, Token token, ParseNodeType represents, int left, int middle, int right);

  string to_string() override;
  NodeManager *get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
  int get_scope_id() override { return scope_id; }
  void set_scope_id(int id) override { scope_id = id; }
  int get_resolved_symbol() override { return resolved_symbol; }
  void set_resolved_symbol(int symbol_idx) override { resolved_symbol = symbol_idx; }
};

class UnaryParseNode : public AbstractParseNode {
public:
  NodeManager *node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  vector<string> lexemes;
  int scope_id = -1;           // Scope this node belongs to
  int resolved_symbol = -1;    // Pre-resolved symbol index (-1 = unresolved)

  int branch;

  UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents);
  UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents, int branch);
  UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents, vector<string> lexemes);

  string to_string() override;
  NodeManager *get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
  int get_scope_id() override { return scope_id; }
  void set_scope_id(int id) override { scope_id = id; }
  int get_resolved_symbol() override { return resolved_symbol; }
  void set_resolved_symbol(int symbol_idx) override { resolved_symbol = symbol_idx; }
};
