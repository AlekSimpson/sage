#include <vector>
#include <string>
#include <cassert>
#include <error_logger.h>

#include "../include/node_manager.h"
#include "../include/scope_manager.h"

#define assertm(condition, message) assert((condition) && (message))

NodeManager::NodeManager() {
    box_count = 0;
    capacity = 500;
    container = new nodebox[500];
    for (int i = 0; i < 500; ++i) {
        container[i] = nodebox{nullptr, PN_UNARY};
        free_spaces.push_back(i);
    }

    root_node = -1;
    scope_manager = nullptr;
}

void NodeManager::set_scope_manager(ScopeManager *sm) {
    scope_manager = sm;
}

NodeManager::~NodeManager() {
    delete[] container;
}

string NodeManager::get_identifier(NodeIndex node) {
    switch (get_host_nodetype(node)) {
        case PN_UNARY: {
            return get_lexeme(node);
        }
        case PN_BINARY:
        case PN_TRINARY: {
            auto left = get_left(node);
            return get_lexeme(left);
        }
        default:
            break;
    }

    return "";
}

int NodeManager::get_node_count() {
    return box_count;
}

BlockParseNode *NodeManager::unbox(NodeIndex index) {
    nodebox box = container[index];

    if (box.host_type == PN_BLOCK) {
        return dynamic_cast<BlockParseNode *>(box.node);
    }

    return nullptr;
}

nodebox NodeManager::get_node(NodeIndex node) {
    assertm(!(node < 0 || node > capacity), sen("get_node: attempted to access invalid node index", node).data());
    return container[node];
}

string NodeManager::to_string(NodeIndex index) {
    auto node = get_node(index).node;
    if (node == nullptr) {
        return "";
    }

    return node->to_string();
}

void NodeManager::showtree(NodeIndex index) {
    auto node = get_node(index).node;
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

    BlockParseNode *node = dynamic_cast<BlockParseNode *>(box.node);
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

    UnaryParseNode *node = dynamic_cast<UnaryParseNode *>(box.node);
    if (node == nullptr) {
        return "";
    }

    if (node->lexemes.size() == 0) {
        return "";
    }

    string full_lex;

    for (int i = 0; i < (int) node->lexemes.size(); ++i) {
        full_lex += node->lexemes[i];
        full_lex += ".";
    }

    full_lex.pop_back();

    return full_lex;
}

NodeIndex NodeManager::get_left(NodeIndex node) {
    auto box = get_node(node);
    assertm(box.node != nullptr, sen("get_left: out of range node index. Given", node,"which is invalid.").data());

    AbstractParseNode *parsenode = box.node;
    auto hosttype = box.host_type;

    if (hosttype == PN_BINARY) {
        BinaryParseNode *binary_node = dynamic_cast<BinaryParseNode *>(parsenode);
        return binary_node->left;
    } else if (hosttype == PN_TRINARY) {
        TrinaryParseNode *trinary_node = dynamic_cast<TrinaryParseNode *>(parsenode);
        return trinary_node->left;
    }

    return NULL_INDEX;
}

NodeIndex NodeManager::get_right(NodeIndex node) {
    auto box = get_node(node);
    assertm(box.node != nullptr, sen("get_right: out of range node index. Given", node,"which is invalid.").data());

    AbstractParseNode *parsenode = box.node;
    auto hosttype = box.host_type;

    if (hosttype == PN_BINARY) {
        BinaryParseNode *binary_node = dynamic_cast<BinaryParseNode *>(parsenode);
        return binary_node->right;
    } else if (hosttype == PN_TRINARY) {
        TrinaryParseNode *trinary_node = dynamic_cast<TrinaryParseNode *>(parsenode);
        return trinary_node->right;
    }

    return NULL_INDEX;
}

NodeIndex NodeManager::get_middle(NodeIndex node) {
    auto box = get_node(node);
    assert(box.node != nullptr);

    AbstractParseNode *parsenode = box.node;
    auto hosttype = box.host_type;

    if (hosttype == PN_TRINARY) {
        TrinaryParseNode *trinary_node = dynamic_cast<TrinaryParseNode *>(parsenode);
        return trinary_node->middle;
    }

    return NULL_INDEX;
}

NodeIndex NodeManager::get_branch(NodeIndex node) {
    auto box = get_node(node);
    assertm(box.node != nullptr, sen("get_branch: out of range node index. Given", node,"which is invalid.").data());

    AbstractParseNode *parsenode = box.node;
    auto hosttype = box.host_type;

    if (hosttype == PN_UNARY) {
        UnaryParseNode *unary_node = dynamic_cast<UnaryParseNode *>(parsenode);
        return unary_node->branch;
    }

    return NULL_INDEX;
}

NodeIndex NodeManager::reach_right(NodeIndex node, int reach_depth) {
    if (reach_depth <= 0) {
        return node;
    }

    auto box = get_node(node);
    if (box.node == nullptr) {
        return NULL_INDEX;
    }

    auto hosttype = box.host_type;
    if (hosttype == PN_BINARY || hosttype == PN_TRINARY) {
        return reach_right(get_right(node), reach_depth - 1);
    }

    return NULL_INDEX;
}

vector<NodeIndex> NodeManager::get_children(NodeIndex node) {
    auto box = get_node(node);
    assertm(box.node != nullptr, sen("get_children: out of range node index. Given", node,"which is invalid.").data());

    AbstractParseNode *parsenode = box.node;
    auto hosttype = box.host_type;

    if (hosttype == PN_BLOCK) {
        BlockParseNode *block_node = dynamic_cast<BlockParseNode *>(parsenode);
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
    BlockParseNode *new_block = new BlockParseNode(this);
    return create(new_block, PN_BLOCK);
}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type) {
    BlockParseNode *new_block = new BlockParseNode(this, token, type);
    auto node_id = create(new_block, PN_BLOCK);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type, vector<NodeIndex> nodes) {
    BlockParseNode *new_block = new BlockParseNode(this, token, type, nodes);
    auto node_id = create(new_block, PN_BLOCK);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_binary(Token token, ParseNodeType type, NodeIndex left, NodeIndex right) {
    BinaryParseNode *binary_node = new BinaryParseNode(this, token, type, left, right);
    auto node_id = create(binary_node, PN_BINARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_empty_binary(Token token, ParseNodeType type) {
    BinaryParseNode *binary_node = new BinaryParseNode(this, token, type, NULL_INDEX, NULL_INDEX);
    auto node_id = create(binary_node, PN_BINARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

void NodeManager::set_binary_left(NodeIndex target, NodeIndex new_left_node) {
    auto box = get_node(target);
    assert(box.node != nullptr);
    assert(box.host_type == PN_BINARY);

    BinaryParseNode *binary_node = dynamic_cast<BinaryParseNode *>(box.node);
    binary_node->left = new_left_node;
}

void NodeManager::set_binary_right(NodeIndex target, NodeIndex new_right_node) {
    auto box = get_node(target);
    assert(box.node != nullptr);
    assert(box.host_type == PN_BINARY);

    BinaryParseNode *binary_node = dynamic_cast<BinaryParseNode *>(box.node);
    binary_node->right = new_right_node;
}

NodeIndex NodeManager::create_trinary(Token token, ParseNodeType type, NodeIndex left, NodeIndex middle,
                                      NodeIndex right) {
    TrinaryParseNode *trinary_node = new TrinaryParseNode(this, token, type, left, middle, right);
    auto node_id = create(trinary_node, PN_TRINARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type) {
    UnaryParseNode *unary_node = new UnaryParseNode(this, token, type);
    auto node_id = create(unary_node, PN_UNARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, NodeIndex branch) {
    UnaryParseNode *unary_node = new UnaryParseNode(this, token, type, branch);
    auto node_id = create(unary_node, PN_UNARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, vector<string> lexemes) {
    UnaryParseNode *unary_node = new UnaryParseNode(this, token, type, lexemes);
    auto node_id = create(unary_node, PN_UNARY);
    set_scope_id(node_id, scope_manager->get_current_scope());
    return node_id;
}

NodeIndex NodeManager::create(AbstractParseNode *node, ParseNodeType hosttype) {
    check_capacity();

    box_count++;

    NodeIndex index = free_spaces.front();
    free_spaces.erase(free_spaces.begin());

    container[index] = nodebox{node, hosttype};

    // Automatically assign scope_id from scope_manager if available
    if (scope_manager != nullptr) {
        node->set_scope_id(scope_manager->get_current_scope());
    }

    return index;
}

NodeIndex NodeManager::get_parent(NodeIndex childnode) {
    return parent_map[childnode];
}

void NodeManager::set_parent(NodeIndex new_parent, NodeIndex new_child) {
    parent_map[new_child] = new_parent;
}

void NodeManager::mark_modified(NodeIndex node) { modified_subtrees.insert(node); }

bool NodeManager::has_modifications() const { return !modified_subtrees.empty(); }

void NodeManager::insert_after(NodeIndex target, NodeIndex new_node) { /* TODO */ }

void NodeManager::remove_node(NodeIndex node) { /* TODO */ }

void NodeManager::replace_node(NodeIndex old_node, NodeIndex new_node) {
    NodeIndex parent = get_parent(old_node);
    if (parent == NULL_INDEX) return;

    auto& pbox = container[parent];

    // Handle binary/trinary node references
    if (pbox.host_type == PN_BINARY) {
        auto* bin = static_cast<BinaryParseNode*>(pbox.node);
        if (bin->left == old_node) bin->left = new_node;
        if (bin->right == old_node) bin->right = new_node;
    } else if (pbox.host_type == PN_TRINARY) {
        auto* tri = static_cast<TrinaryParseNode*>(pbox.node);
        if (tri->left == old_node) tri->left = new_node;
        if (tri->middle == old_node) tri->middle = new_node;
        if (tri->right == old_node) tri->right = new_node;
    }

    // Handle block children
    if (pbox.host_type == PN_BLOCK) {
        auto* blk = static_cast<BlockParseNode*>(pbox.node);
        for (auto& child : blk->children) {
            if (child == old_node) child = new_node;
        }
    }

    set_parent(new_node, parent);
    mark_modified(parent);
}

void NodeManager::splice_nodes(NodeIndex target, vector<NodeIndex> replacements) {
    NodeIndex parent = get_parent(target);
    if (parent == NULL_INDEX || get_host_nodetype(parent) != PN_BLOCK) return;

    auto* blk = static_cast<BlockParseNode*>(container[parent].node);
    vector<NodeIndex> new_children;

    for (NodeIndex child : blk->children) {
        if (child == target) {
            for (NodeIndex rep : replacements) {
                new_children.push_back(rep);
                set_parent(rep, parent);
            }
        } else {
            new_children.push_back(child);
        }
    }

    blk->children = std::move(new_children);
    mark_modified(parent);
}

void NodeManager::expand_container() {
    int old_capacity = capacity;
    capacity = capacity * 2;

    nodebox *new_container = new nodebox[capacity];

    for (int i = 0; i < capacity; i++) {
        if (i < old_capacity) {
            // copy old nodes into new container
            new_container[i] = container[i];
            continue;
        }

        // otherwise we are initializing newly created space to null
        // and updating the amount of free space
        new_container[i] = nodebox{nullptr, PN_UNARY};
        free_spaces.push_back(i);
    }

    delete[] container;
    container = nullptr;

    container = new_container;
}

void NodeManager::set_branch(NodeIndex parent, NodeIndex child) {
    if (get_host_nodetype(parent) != PN_UNARY) return;

    nodebox box = get_node(parent);
    UnaryParseNode *unary = (UnaryParseNode *)box.node;

    unary->branch = child;
}

void NodeManager::set_children(NodeIndex blocknode, vector<NodeIndex> new_children) {
    if (get_host_nodetype(blocknode) != PN_BLOCK) {
        return;
    }

    auto block = unbox(blocknode);

    block->children = new_children;
}

void NodeManager::set_scope_id(NodeIndex node, int scope_id) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        return;
    }
    box.node->set_scope_id(scope_id);
}

int NodeManager::get_scope_id(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        return -1;
    }
    return box.node->get_scope_id();
}

set<int> NodeManager::get_in_scope_symbol_indices(int scope_id) {
    set<int> in_scope_symbol_indices;
    auto &scopes = scope_manager->scopes;

    int _current_scope_id = scope_id;
    while (_current_scope_id >= 0) {
        in_scope_symbol_indices.insert(scopes[_current_scope_id].symbol_indices.begin(), scopes[_current_scope_id].symbol_indices.end());
        _current_scope_id--;
    }
    return in_scope_symbol_indices;
}

void NodeManager::set_resolved_symbol(NodeIndex node, int symbol_index) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        return;
    }
    box.node->set_resolved_symbol(symbol_index);
}

int NodeManager::get_resolved_symbol(NodeIndex node) {
    auto box = get_node(node);
    if (box.node == nullptr) {
        return -1;
    }
    return box.node->get_resolved_symbol();
}
