#include <string>
#include "../include/parse_node.h"
using namespace std;

string nodetype_to_string(ParseNodeType nodetype) {
    switch (nodetype) {
    case PN_BINARY:
        return "BINARY";
    case PN_TRINARY:
    	return "TRINARY";
    case PN_UNARY:
    	return "UNARY";
    case PN_NUMBER:
    	return "NUMBER";
    case PN_FLOAT:
    	return "FLOAT";
    case PN_IDENTIFIER:
    	return "IDENTIFIER";
    case PN_KEYWORD:
    	return "KEYWORD";
    case PN_BLOCK:
    	return "BLOCK";
    case PN_CODE_BLOCK:
    	return "CODE_BLOCK";
    case PN_PARAM_LIST:
    	return "PARAM_LIST";
    case PN_FUNCDEF:
    	return "FUNCDEF";
    case PN_FUNCCALL:
    	return "FUNCCALL";
    case PN_TYPE:
    	return "TYPE";
    case PN_STRUCT:
    	return "STRUCT";
    case PN_IF:
    	return "IF";
    case PN_IF_BRANCH:
    	return "IF_BRANCH";
    case PN_ELSE_BRANCH:
    	return "ELSE_BRANCH";
    case PN_WHILE:
    	return "WHILE";
    case PN_ASSIGN:
    	return "ASSIGN";
    case PN_FOR:
    	return "FOR";
    case PN_PROGRAM:
    	return "PROGRAM";
    case PN_RANGE:
    	return "RANGE";
    case PN_VAR_DEC:
    	return "VAR_DEC";
    case PN_VAR_REF:
    	return "VAR_REF";
    case PN_STRING:
    	return "STRING";
    case PN_RUN_DIRECTIVE:
    	return "RUN_DIRECTIVE";
    case PN_LIST:
    	return "LIST";
    case PN_INCLUDE:
    	return "INCLUDE";
    case PN_VARARG:
    	return "VARARG";
    case PN_FUNCDEC:
    	return "FUNCDEC";
    default:
    	return "Unknown Node Type (Could have forgot to add String() impl for new type)";
    }
}

// SECTION: BlockParseNode definitions
BlockParseNode::BlockParseNode() {
    children = vector<AbstractParseNode*>();
}

BlockParseNode::BlockParseNode(Token token, ParseNodeType represents) {
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    children = vector<AbstractParseNode*>();
}

BlockParseNode::BlockParseNode(Token token, ParseNodeType represents, vector<AbstractParseNode*> children) {
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    children = children;
}

BlockParseNode::~BlockParseNode() {
    if (!children.empty()) {
        for (int i = 0; i < (int)children.size(); ++i) {
            if (children.at(i) == nullptr)
                continue;
            delete children.at(i);
            children.insert(children.begin() + i, nullptr);
        }
    }
}

string BlockParseNode::to_string() {
    return "BLOCK";
}

void BlockParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());
    printf("%d\n", (int)children.size());

    for (int i = 0; i < (int)children.size(); ++i) {
        children.at(i)->showtree(depth + "\t");
    }
}

Token BlockParseNode::get_token() {
    return token;
}

TokenType BlockParseNode::get_token_type() {
    return token.token_type;
}

AbstractParseNode* BlockParseNode::get_child_node() {
    if (children.size() == 0) {
        return nullptr;
    }

    return children[0];
}

ParseNodeType BlockParseNode::get_nodetype() {
    return rep_nodetype;
}

ParseNodeType BlockParseNode::get_host_nodetype() {
    return host_nodetype;
}

// SECTION: BinaryParseNode definitions

BinaryParseNode::BinaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* left, AbstractParseNode* right) {
    this->token = token;
    rep_nodetype = represents;
    host_nodetype = PN_BINARY;
    this->left = left;
    this->right = right;
}

BinaryParseNode::~BinaryParseNode() {
    if (left != nullptr) {
        delete left;
    }

    if (right != nullptr) {
        delete right;
    }
}

string BinaryParseNode::to_string() {
    char buffer[200];

    snprintf(buffer, sizeof(buffer), "BinaryNode{%s | %s | %s | %s}", nodetype_to_string(rep_nodetype).c_str(), token.lexeme.c_str(), left->to_string().c_str(), right->to_string().c_str());

    string retval;
    retval = buffer;

    return retval;
}

void BinaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    if (left != nullptr) {
        left->showtree(depth + "\t");
    }

    if (right != nullptr) {
        right->showtree(depth + "\t");
    }
}

Token BinaryParseNode::get_token() {
    return token;
}

TokenType BinaryParseNode::get_token_type() {
    return token.token_type;
}

AbstractParseNode* BinaryParseNode::get_child_node() {
    return left;
}

ParseNodeType BinaryParseNode::get_nodetype() {
    return rep_nodetype;
}

ParseNodeType BinaryParseNode::get_host_nodetype() {
    return host_nodetype;
}

// SECTION: TrinaryParseNode definitions

TrinaryParseNode::TrinaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* left, AbstractParseNode* middle, AbstractParseNode* right) {
    this->token = token;
    host_nodetype = PN_TRINARY;
    rep_nodetype = represents;
    this->left = left;
    this->middle = middle;
    this->right = right;
}

TrinaryParseNode::~TrinaryParseNode() {
    if (left != nullptr) {
        delete left;
    }

    if (middle != nullptr) {
        delete middle;
    }

    if (right != nullptr) {
        delete right;
    }
}


string TrinaryParseNode::to_string() {
    char buffer[200];

    snprintf(buffer, sizeof(buffer), "BinaryNode{%s | %s | %s | %s | %s}", nodetype_to_string(rep_nodetype).c_str(), token.lexeme.c_str(), left->to_string().c_str(), middle->to_string().c_str(), right->to_string().c_str());

    string retval;
    retval = buffer;

    return retval;
}

void TrinaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    if (left != nullptr) {
        left->showtree(depth + "\t");
    }

    if (middle != nullptr) {
        middle->showtree(depth + "\t");
    }

    if (right != nullptr) {
        right->showtree(depth + "\t");
    }
}

Token TrinaryParseNode::get_token() {
    return token;
}

TokenType TrinaryParseNode::get_token_type() {
    return token.token_type;
}

AbstractParseNode* TrinaryParseNode::get_child_node() {
    return left;
}

ParseNodeType TrinaryParseNode::get_nodetype() {
    return rep_nodetype;
}

ParseNodeType TrinaryParseNode::get_host_nodetype() {
    return host_nodetype;
}

// SECTION: UnaryParseNode definitions

UnaryParseNode::UnaryParseNode() {
    branch = nullptr;
}

UnaryParseNode::UnaryParseNode(Token token, ParseNodeType represents) {
    this->token = token;
    host_nodetype = PN_UNARY;
    rep_nodetype = represents;
    branch = nullptr;
}

UnaryParseNode::UnaryParseNode(Token token, ParseNodeType represents, AbstractParseNode* branch) {
    this->token = token;
    host_nodetype = PN_UNARY;
    rep_nodetype = represents;
    this->branch = branch;
}

UnaryParseNode::UnaryParseNode(Token token, ParseNodeType represents, vector<string> lexemes) {
    this->token = token;
    host_nodetype = PN_UNARY;
    rep_nodetype = represents;
    this->lexemes = lexemes;
}

UnaryParseNode::~UnaryParseNode() {
    if (branch != nullptr) {
        delete branch;
    }
}

string UnaryParseNode::get_full_lexeme() {
    if (lexemes.size() == 0) {
        return "";
    }

    string full_lex;

    for (int i = 0; i < (int)lexemes.size(); ++i) {
        full_lex += lexemes[i];
        full_lex += ".";
    }

    full_lex.pop_back();

    return full_lex;
}

string UnaryParseNode::to_string() {
    char buffer[100];

    if (branch != nullptr) {
        snprintf(buffer, sizeof(buffer), "UnaryNode{%s | %s | %s}", nodetype_to_string(rep_nodetype).c_str(), token.lexeme.c_str(), branch->to_string().c_str());
    } else {
        snprintf(buffer, sizeof(buffer), "UnaryNode{%s | %s}", nodetype_to_string(rep_nodetype).c_str(), token.lexeme.c_str());
    }

    string retval;
    retval = buffer;

    return retval;
}

void UnaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    if (branch != nullptr) {
        branch->showtree(depth + "\t");
    }
}

Token UnaryParseNode::get_token() {
    return token;
}

TokenType UnaryParseNode::get_token_type() {
    return token.token_type;
}

AbstractParseNode* UnaryParseNode::get_child_node() {
    return branch;
}

ParseNodeType UnaryParseNode::get_nodetype() {
    return rep_nodetype;
}

ParseNodeType UnaryParseNode::get_host_nodetype() {
    return host_nodetype;
}



