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
        case PN_STATIC_ARRAY_TYPE:
            return "STATIC_ARRAY_TYPE";
        case PN_DYNAMIC_ARRAY_TYPE:
            return "DYNAMIC_ARRAY_TYPE";
        case PN_POINTER_TYPE:
            return "POINTER_TYPE";
        case PN_ARRAY_REFERENCE_TYPE:
            return "ARRAY_REFERENCE_TYPE";
        default:
            return "Unknown Node Type (Could have forgot to add String() impl for new type)";
    }
}

// SECTION: BlockParseNode definitions
BlockParseNode::BlockParseNode(NodeManager *man) {
    node_manager = man;
    children = vector<NodeIndex>();
    host_nodetype = PN_BLOCK;
    rep_nodetype = PN_BLOCK;
    this->token = Token();
}

BlockParseNode::BlockParseNode(NodeManager *man, Token token, ParseNodeType represents) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    children = vector<NodeIndex>();
}

BlockParseNode::BlockParseNode(NodeManager *man, Token token, ParseNodeType represents, vector<NodeIndex> children) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_BLOCK;
    rep_nodetype = represents;
    this->children = children;
}

void BlockParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    for (int i = 0; i < (int) children.size(); ++i) {
        if (children[i] < 0) {
            printf("invalid child ptr: %d\n", children[i]);
            continue;
        }

        auto child = node_manager->get_node(children[i]).node;
        if (child == nullptr) {
            printf("boxchild ptr was null\n");
            continue;
        }

        child->showtree(depth + "\t");
    }
}

ParseNodeType BlockParseNode::get_rep_nodetype() {
    return rep_nodetype;
}

ParseNodeType BlockParseNode::get_host_nodetype() {
    return host_nodetype;
}

Token BlockParseNode::get_token() {
    return token;
}

string BlockParseNode::to_string() {
    return "BLOCK";
}

NodeManager *BlockParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: BinaryParseNode definitions

BinaryParseNode::BinaryParseNode(NodeManager *man, Token token, ParseNodeType represents, NodeIndex left,
                                 NodeIndex right) {
    node_manager = man;
    this->token = token;
    rep_nodetype = represents;
    host_nodetype = PN_BINARY;
    this->left = left;
    this->right = right;
}

void BinaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    AbstractParseNode *child;
    if (left != NULL_INDEX) {
        child = node_manager->get_node(left).node;
        child->showtree(depth + "\t");
    }

    if (right != NULL_INDEX) {
        child = node_manager->get_node(right).node;
        child->showtree(depth + "\t");
    }
}

ParseNodeType BinaryParseNode::get_rep_nodetype() {
    return rep_nodetype;
}

ParseNodeType BinaryParseNode::get_host_nodetype() {
    return host_nodetype;
}

Token BinaryParseNode::get_token() {
    return token;
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

NodeManager *BinaryParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: TrinaryParseNode definitions

TrinaryParseNode::TrinaryParseNode(NodeManager *man, Token token, ParseNodeType represents, NodeIndex left,
                                   NodeIndex middle, NodeIndex right) {
    node_manager = man;
    this->token = token;
    host_nodetype = PN_TRINARY;
    rep_nodetype = represents;
    this->left = left;
    this->middle = middle;
    this->right = right;
}

void TrinaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    AbstractParseNode *child;
    if (left != NULL_INDEX) {
        child = node_manager->get_node(left).node;
        child->showtree(depth + "\t");
    }

    if (middle != NULL_INDEX) {
        child = node_manager->get_node(middle).node;
        child->showtree(depth + "\t");
    }

    if (right != NULL_INDEX) {
        child = node_manager->get_node(right).node;
        child->showtree(depth + "\t");
    }
}

ParseNodeType TrinaryParseNode::get_rep_nodetype() {
    return rep_nodetype;
}

ParseNodeType TrinaryParseNode::get_host_nodetype() {
    return host_nodetype;
}

Token TrinaryParseNode::get_token() {
    return token;
}

string TrinaryParseNode::to_string() {
    char buffer[200];

    string left_string = node_manager->to_string(left);
    string middle_string = node_manager->to_string(middle);
    string right_string = node_manager->to_string(right);

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

NodeManager *TrinaryParseNode::get_node_manager() {
    return node_manager;
}

// SECTION: UnaryParseNode definitions

UnaryParseNode::UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->branch = -1;
}

UnaryParseNode::UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents, NodeIndex branch) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->branch = branch;
}

UnaryParseNode::UnaryParseNode(NodeManager *man, Token token, ParseNodeType represents, vector<string> lexemes) {
    node_manager = man;
    this->token = token;
    this->host_nodetype = PN_UNARY;
    this->rep_nodetype = represents;
    this->lexemes = lexemes;
    this->branch = -1;
}

void UnaryParseNode::showtree(string depth) {
    printf("%s- %s\n", depth.c_str(), this->to_string().c_str());

    //UnaryParseNode* child;
    if (branch != NULL_INDEX) {
        auto child = node_manager->get_node(branch).node;
        //child = dynamic_cast<UnaryParseNode*>(node_manager->get_node(branch).node);
        child->showtree(depth + "\t");
    }
}

Token UnaryParseNode::get_token() {
    return token;
}

ParseNodeType UnaryParseNode::get_rep_nodetype() {
    return rep_nodetype;
}

ParseNodeType UnaryParseNode::get_host_nodetype() {
    return host_nodetype;
}

string UnaryParseNode::to_string() {
    char buffer[100];

    if (branch != NULL_INDEX) {
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

NodeManager *UnaryParseNode::get_node_manager() {
    return node_manager;
}
