#include <vector>
#include <string>
#include "../include/parse_node.h"

NodeManager::NodeManager() {
    root_node = -1;
}

NodeManager::~NodeManager() {
    for (auto box : container) {
        delete box.node;
    }
}

string NodeManager::to_string(NodeIndex node) {
    if (node < 0 || node > conatiner.size()) {
        printf("WARNING | to_string: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return "";
    }

    auto node = container.at(node).node;
    return node->to_string();
}

void Node::Manager::showtree(NodeIndex node) {
    if (node < 0 || node > conatiner.size()) {
        printf("WARNING | showtree: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return "";
    }

    auto node = container.at(node);
    node->showtree("");
}

ParseNodeType NodeManager::get_nodetype(NodeIndex node) {
    if (node < 0 || node > conatiner.size()) {
        printf("WARNING | get_nodetype: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return 0;
    }

    auto node = container.at(node);
}

ParseNodeType NodeManager::get_host_nodetype(NodeIndex node) {}

NodeIndex NodeManager::get_left(NodeIndex node) {}
NodeIndex NodeManager::get_right(NodeIndex node) {}
NodeIndex NodeManager::get_middle(NodeIndex node) {}
NodeIndex NodeManager::get_branch(NodeIndex node) {}
vector<NodeIndex> NodeManager::get_children(NodeIndex node) {}

void NodeManager::delete_node(NodeIndex index) {}

NodeIndex NodeManager::create_block() {}
NodeIndex NodeManager::create_block(Token token, ParseNodeType type) {}
NodeIndex NodeManager::create_block(Token token, ParseNodeType type, vector<NodeIndex> nodes) {}

NodeIndex NodeManager::create_binary(Token token, ParseNodeType type, NodeIndex left, NodeIndex right) {}

NodeIndex NodeManager::create_trinary(Token token, ParseNodeType type, NodeIndex left, NodeIndex middle, NodeIndex right) {}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type) {}
NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, NodeIndex branch) {}
NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, vector<string> lexemes) {}
