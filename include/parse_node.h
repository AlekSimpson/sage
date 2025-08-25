#pragma once

#include <string>
#include <vector>
#include "token.h"
using namespace std;

class NodeManager;

typedef enum {
  PN_BINARY,
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
  PN_VARARG,
  PN_FUNCDEC,
} ParseNodeType;

string nodetype_to_string(ParseNodeType nodetype);

class AbstractParseNode {
public:
  virtual ~AbstractParseNode() {}
  virtual string to_string() = 0;
  virtual void showtree(string depth) = 0;
  virtual NodeManager* get_node_manager() = 0;
  virtual Token get_token() = 0;
  virtual ParseNodeType get_rep_nodetype() = 0;
  virtual ParseNodeType get_host_nodetype() = 0;
};

class BlockParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  vector<int> children;

  BlockParseNode(NodeManager* man);
  BlockParseNode(NodeManager* man, Token token, ParseNodeType represents);
  BlockParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<int> children);

  string to_string() override;
  NodeManager* get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
};

class BinaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  int left;
  int right;

  BinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, int left, int right);

  string to_string() override;
  NodeManager* get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
};

class TrinaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  int left;
  int middle;
  int right;

  TrinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, int left, int middle, int right);

  string to_string() override;
  NodeManager* get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
};

class UnaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  vector<string> lexemes;

  int branch;

  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents);
  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, int branch);
  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<string> lexemes);

  string to_string() override;
  NodeManager* get_node_manager() override;
  void showtree(string depth) override;
  Token get_token() override;
  ParseNodeType get_rep_nodetype() override;
  ParseNodeType get_host_nodetype() override;
};
