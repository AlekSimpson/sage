#ifndef PARSE_NODE
#define PARSE_NODE

#include <string>
#include <vector>
#include "../include/token.h"
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
  PN_CODE_BLOCK,
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
  virtual ~AbstractParseNode() {};
  virtual string to_string() = 0;
  virtual void showtree(string depth) = 0;
  virtual Token get_token() = 0;
  virtual TokenType get_token_type() = 0;
  virtual AbstractParseNode* get_child_node() = 0;
};

class BlockParseNode : public AbstractParseNode {
public:
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  AbstractParseNode** children;
  int children_amount;

  BlockParseNode();
  BlockParseNode(Token token, ParseNodeType host, ParseNodeType represents);
  BlockParseNode(Token token, AbstractParseNode** children, int children_amount);
  ~BlockParseNode();

  string to_string() override;
  void showtree(string depth) override;
  Token get_token() override;
  TokenType get_token_type() override;
  AbstractParseNode* get_child_node() override;
};

class BinaryParseNode : public AbstractParseNode {
public:
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  AbstractParseNode* left;
  AbstractParseNode* right;

  BinaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* left, AbstractParseNode* right);
  ~BinaryParseNode();

  string to_string() override;
  void showtree(string depth) override;
  Token get_token() override;
  TokenType get_token_type() override;
  AbstractParseNode* get_child_node() override;
};

class TrinaryParseNode : public AbstractParseNode {
public:
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)

  AbstractParseNode* left;
  AbstractParseNode* middle;
  AbstractParseNode* right;

  TrinaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* left, AbstractParseNode* middle, AbstractParseNode* right);
  ~TrinaryParseNode();

  string to_string() override;
  void showtree(string depth) override;
  Token get_token() override;
  TokenType get_token_type() override;
  AbstractParseNode* get_child_node() override;
};

class UnaryParseNode : public AbstractParseNode {
public:
  Token token;
  ParseNodeType host_nodetype; // identifies host cpp structure type
  ParseNodeType rep_nodetype; // identifies the semantic node type (what the node is representing)
  vector<string> lexemes;

  AbstractParseNode* branch;

  UnaryParseNode();
  UnaryParseNode(Token token, ParseNodeType represents);
  UnaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* branch);
  ~UnaryParseNode();

  string get_full_lexeme();
  string to_string() override;
  void showtree(string depth) override;
  Token get_token() override;
  TokenType get_token_type() override;
  AbstractParseNode* get_child_node() override;
};

#endif
