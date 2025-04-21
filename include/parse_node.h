#pragma once

#include <string>
#include <vector>
// #include "../include/token.h"
#include "token.h"
#include "node_manager.h"
using namespace std;

typedef enum {
  PN_BINARY,
  PN_TRINARY,
  PN_UNARY,
  PN_NUMBER,
  PN_FLOAT,
  PN_STRING,
  PN_IDENTIFIER,
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
  PN_PROGRAM,
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
  virtual string to_string() = 0;
  virtual NodeManager* node_manager() = 0;
};

class BlockParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  vector<NodeIndex> children;

  BlockParseNode();
  BlockParseNode(NodeManager* man, Token token, ParseNodeType represents);
  BlockParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<NodeIndex> children);

  string to_string() override;
  NodeManager* node_manager() override;
};

class BinaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  NodeIndex left;
  NodeIndex right;

  BinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex left, NodeIndex right);

  string to_string() override;
  NodeManager* node_manager() override;
};

class TrinaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  NodeIndex left;
  NodeIndex middle;
  NodeIndex right;

  TrinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex left, NodeIndex middle, NodeIndex right);

  string to_string() override;
  NodeManager* node_manager() override;
};

class UnaryParseNode : public AbstractParseNode {
public:
  NodeManager* node_manager;
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  vector<string> lexemes;

  NodeIndex branch;

  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents);
  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex branch);
  UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<string> lexemes);

  string to_string() override;
  NodeManager* node_manager() override;
};
