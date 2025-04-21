#pragma once

#include <vector>
#include <string>
#include "parse_node.h"

typedef NodeIndex int;

struct nodebox {
    AbstractParseNode* node;
    ParseNodeType host_type;
};

class NodeManager {
private:
    vector<nodebox> container;
    NodeIndex root_node;

public:
    ~NodeManager();
    NodeManager();

    string to_string(NodeIndex);
    void showtree(NodeIndex);
    ParseNodeType get_nodetype(NodeIndex);
    ParseNodeType get_host_nodetype(NodeIndex);

    NodeIndex get_left(NodeIndex);
    NodeIndex get_right(NodeIndex);
    NodeIndex get_middle(NodeIndex);
    NodeIndex get_branch(NodeIndex);
    vector<NodeIndex> get_children(NodeIndex);

    void delete_node(NodeIndex index);

    NodeIndex create_block();
    NodeIndex create_block(Token, ParseNodeType);
    NodeIndex create_block(Token, ParseNodeType, vector<NodeIndex>);

    NodeIndex create_binary(Token, ParseNodeType, NodeIndex, NodeIndex);

    NodeIndex create_trinary(Token, ParseNodeType, NodeIndex, NodeIndex, NodeIndex);

    NodeIndex create_unary(Token, ParseNodeType);
    NodeIndex create_unary(Token, ParseNodeType, NodeIndex branch);
    NodeIndex create_unary(Token, ParseNodeType, vector<string> lexemes);
}
