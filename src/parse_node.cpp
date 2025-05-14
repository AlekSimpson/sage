#include <string>
#include <vector>
#include "../include/parse_node.h"
#include "../include/node_manager.h"

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
BlockParseNode::BlockParseNode(NodeManager* man) {
    node_manager = man;
    children = vector<NodeIndex>();
    host_nodetype = PN_BLOCK;
    rep_nodetype = PN_BLOCK;
    this->token = Token();
}

BlockParseNode::BlockParseNode(NodeManager* man, Token token, ParseNodeType represents) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    children = vector<NodeIndex>();
}

BlockParseNode::BlockParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<NodeIndex> children) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    this->children = children;
}

string BlockParseNode::to_string() {
    return "BLOCK";
}

NodeManager* BlockParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: BinaryParseNode definitions

BinaryParseNode::BinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex left, NodeIndex right) {
    node_manager = man;
    this->token = token;
    rep_nodetype = represents;
    host_nodetype = PN_BINARY;
    this->left = left;
    this->right = right;
}

string BinaryParseNode::to_string() {
    char buffer[200];

    string left_string = node_manager->to_string(left);
    string right_string = node_manager->to_string(right);

    snprintf(buffer, 
             sizeof(buffer), 
             "BinaryNode{%s | %s | %s | %s}", 
             nodetype_to_string(rep_nodetype).c_str(), 
             token.lexeme.c_str(), 
             left_string.c_str(), 
             right_string.c_str());

    string retval;
    retval = buffer;

    return retval;
}

NodeManager* BinaryParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: TrinaryParseNode definitions

TrinaryParseNode::TrinaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex left, NodeIndex middle, NodeIndex right) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_TRINARY;
    rep_nodetype = represents;
    this->left = left;
    this->middle = middle;
    this->right = right;
}

string TrinaryParseNode::to_string() {
    char buffer[200];

    string left_string = node_manager->to_string(left);
    string middle_string = node_manager->to_string(middle);
    srting right_string = node_manager->to_string(right);

    snprintf(buffer, 
             sizeof(buffer), 
             "TrinaryNode{%s | %s | %s | %s | %s}",
             nodetype_to_string(rep_nodetype).c_str(),
             token.lexeme.c_str(),
             left_string.c_str(),
             middle_string.c_str(),
             right_string.c_str()
        );

    string retval;
    retval = buffer;

    return retval;
}

NodeManager* TrinaryParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: UnaryParseNode definitions

UnaryParseNode::UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->branch = -1;
}

UnaryParseNode::UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, NodeIndex branch) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->branch = branch;
}

UnaryParseNode::UnaryParseNode(NodeManager* man, Token token, ParseNodeType represents, vector<string> lexemes) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->lexemes = lexemes;
    this->branch = -1;
}

// string UnaryParseNode::get_full_lexeme() {
//     if (lexemes.size() == 0) {
//         return "";
//     }
// 
//     string full_lex;
// 
//     for (int i = 0; i < (int)lexemes.size(); ++i) {
//         full_lex += lexemes[i];
//         full_lex += ".";
//     }
// 
//     full_lex.pop_back();
// 
//     return full_lex;
// }

string UnaryParseNode::to_string() {
    char buffer[100];

    if (branch != nullptr) {
        string branch_string = node_manager->to_string(branch);

        snprintf(buffer, 
                 sizeof(buffer), 
                 "UnaryNode{%s | %s | %s}",
                 nodetype_to_string(rep_nodetype).c_str(),
                 token.lexeme.c_str(), branch_string.c_str());
    } else {
        snprintf(buffer, 
                 sizeof(buffer),
                 "UnaryNode{%s | %s}",
                 nodetype_to_string(rep_nodetype).c_str(), token.lexeme.c_str());
    }

    string retval;
    retval = buffer;

    return retval;
}

NodeManager* UnaryParseNode::get_node_manager() {
    return node_manager;
}

