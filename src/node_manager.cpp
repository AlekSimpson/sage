#include <vector>
#include <string>
#include <iostream>
#include <typeinfo>
#include "../include/node_manager.h"

NodeManager::NodeManager() {
    box_count = 0;
    capacity = 500;
    container = new nodebox[500];
    for (int i = 0; i < 500; i++) {
        container[i] = nodebox{nullptr, PN_UNARY};
        free_spaces.push_back(i);
    }

    root_node = -1;
}

NodeManager::~NodeManager() {
    delete[] container;
}

BlockParseNode* NodeManager::unbox(NodeIndex index) {
    nodebox box = container[index];

    if (box.host_type == PN_BLOCK) {
        return dynamic_cast<BlockParseNode*>(box.node);
    }

    return nullptr;
}

nodebox NodeManager::get_node(NodeIndex node) {
    if (node < 0 || node > capacity) {
        printf("WARNING: accessing invalid node index %d\n", node);
        return nodebox{};
    }
    
    nodebox box = container[node];
    return box;
}

string NodeManager::to_string(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return "";
    }
    
    return node->to_string();
}

void NodeManager::showtree(NodeIndex index) {
    nodebox box = get_node(index);
    auto node = box.node;
    if (node == nullptr) {
        return;
    }
    
    node->showtree("");
}

ParseNodeType NodeManager::get_nodetype(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return PN_UNARY;
    }

    return node->get_rep_nodetype();
}

ParseNodeType NodeManager::get_host_nodetype(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return PN_UNARY;
    }
    return node->get_host_nodetype();
}

string NodeManager::get_lexeme(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return "";
    }

    return node->get_token().lexeme;
}

Token NodeManager::get_token(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return Token(); 
    }

    return node->get_token();
}

void NodeManager::add_child(NodeIndex index, NodeIndex new_child) {
    nodebox box = get_node(index);
    if (box.host_type != PN_BLOCK) {
        return;
    }

    BlockParseNode* node = dynamic_cast<BlockParseNode*>(box.node);
    if (node == nullptr || new_child < 0 || new_child >= capacity) {
        return;
    }
    
    node->children.push_back(new_child);
}

string NodeManager::get_full_lexeme(NodeIndex index) {
    nodebox box = get_node(index);
    if (box.host_type != PN_UNARY) {
        return "";
    }

    UnaryParseNode* node = dynamic_cast<UnaryParseNode*>(box.node);
    if (node == nullptr) {
        return "";
    }

    if (node->lexemes.size() == 0) {
        return "";
    }

    string full_lex;

    for (int i = 0; i < (int)node->lexemes.size(); ++i) {
        full_lex += node->lexemes[i];
        full_lex += ".";
    }

    full_lex.pop_back();

    return full_lex;
}

NodeIndex NodeManager::get_left(NodeIndex index) {
    auto box = get_node(index);
    if (box.node == nullptr) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", index);
        return NULL_INDEX;
    }

    AbstractParseNode* parsenode = box.node;
    auto hosttype = box.host_type;
    
    if (hosttype == PN_BINARY) {
        BinaryParseNode* binary_node = dynamic_cast<BinaryParseNode*>(parsenode);
        return binary_node->left;
    
    }else if (hosttype == PN_TRINARY) {
        TrinaryParseNode* trinary_node = dynamic_cast<TrinaryParseNode*>(parsenode);
        return trinary_node->left;
    }
    
    return NULL_INDEX;
}

NodeIndex NodeManager::get_right(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return NULL_INDEX;
    }

    AbstractParseNode* parsenode = box.node;
    auto hosttype = box.host_type;
    
    if (hosttype == PN_BINARY) {
        BinaryParseNode* binary_node = dynamic_cast<BinaryParseNode*>(parsenode);
        return binary_node->right;
    
    }else if (hosttype == PN_TRINARY) {
        TrinaryParseNode* trinary_node = dynamic_cast<TrinaryParseNode*>(parsenode);
        return trinary_node->right;
    }
    
    return NULL_INDEX;
}

NodeIndex NodeManager::get_middle(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return NULL_INDEX;
    }

    AbstractParseNode* parsenode = box.node;
    auto hosttype = box.host_type;
    
    if (hosttype == PN_TRINARY) {
        TrinaryParseNode* trinary_node = dynamic_cast<TrinaryParseNode*>(parsenode);
        return trinary_node->middle;
    }
    
    return NULL_INDEX;
}

NodeIndex NodeManager::get_branch(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return NULL_INDEX;
    }

    AbstractParseNode* parsenode = box.node;
    auto hosttype = box.host_type;
    
    if (hosttype == PN_UNARY) {
        UnaryParseNode* unary_node = dynamic_cast<UnaryParseNode*>(parsenode);
        return unary_node->branch;
    }
    
    return NULL_INDEX;
}

vector<NodeIndex> NodeManager::get_children(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", node);
        return vector<NodeIndex>();
    }

    AbstractParseNode* parsenode = box.node;
    auto hosttype = box.host_type;
    
    if (hosttype == PN_BLOCK) {
        BlockParseNode* block_node = dynamic_cast<BlockParseNode*>(parsenode);
        return block_node->children;
    }
    
    return vector<NodeIndex>();
}

void NodeManager::delete_node(NodeIndex index) {
    delete container[index].node;
    container[index].node = nullptr;
    
    free_spaces.push_back(index);
    box_count--;
}

void NodeManager::check_capacity() {
    if (box_count == capacity) {
        expand_container();
    }
}

NodeIndex NodeManager::create_block() {
    BlockParseNode* new_block = new BlockParseNode(this);
    return create(new_block, PN_BLOCK);
}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type) {
    BlockParseNode* new_block = new BlockParseNode(this, token, type);
    return create(new_block, PN_BLOCK);
}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type, vector<NodeIndex> nodes) {
    BlockParseNode* new_block = new BlockParseNode(this, token, type, nodes);
    return create(new_block, PN_BLOCK);
}

NodeIndex NodeManager::create_binary(Token token, ParseNodeType type, NodeIndex left, NodeIndex right) {
    BinaryParseNode* binary_node = new BinaryParseNode(this, token, type, left, right); 
    return create(binary_node, PN_BINARY);
}

NodeIndex NodeManager::create_trinary(Token token, ParseNodeType type, NodeIndex left, NodeIndex middle, NodeIndex right) {
    TrinaryParseNode* trinary_node = new TrinaryParseNode(this, token, type, left, middle, right);
    return create(trinary_node, PN_TRINARY);
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type) {
    UnaryParseNode* unary_node = new UnaryParseNode(this, token, type);
    return create(unary_node, PN_UNARY);
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, NodeIndex branch) {
    UnaryParseNode* unary_node = new UnaryParseNode(this, token, type, branch);
    return create(unary_node, PN_UNARY);
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, vector<string> lexemes) {
    UnaryParseNode* unary_node = new UnaryParseNode(this, token, type, lexemes);
    return create(unary_node, PN_UNARY);
}

NodeIndex NodeManager::create(AbstractParseNode* node, ParseNodeType hosttype) {
    check_capacity();

    box_count++;

    NodeIndex index = free_spaces.front();
    free_spaces.erase(free_spaces.begin());

    container[index] = nodebox{node, hosttype};

    return index;
}

void NodeManager::expand_container() {
    int old_capacity = capacity;
    capacity = capacity * 2;
    
    nodebox* new_container = new nodebox[capacity];
    
    for (int i = 0; i < capacity; i++) {
        if (i < old_capacity) {
            // copy old nodes into new container
            new_container[i] = container[i];
            continue;
        }

        // otherwise we are initializing newly created space to null 
        // and updating the amount of free space
        container[i] = nodebox{nullptr, PN_UNARY};
        free_spaces.push_back(i);
    }
    
    delete[] container;
    container = nullptr;
    
    container = new_container;
}



