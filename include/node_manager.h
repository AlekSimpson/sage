#pragma once

#include <string>
#include "parse_node.h"
#include "scope_manager.h"

#define NULL_INDEX -1

typedef int NodeIndex;

struct nodebox {
    AbstractParseNode* node = nullptr;
    ParseNodeType host_type = PN_UNARY;
};

class NodeManager {
private:
    nodebox *container;
    int box_count;
    int capacity;
    vector<NodeIndex> free_spaces;

    NodeIndex root_node;
    ScopeManager *scope_manager;  // Reference for auto-assigning scope IDs

public:
    ~NodeManager();
    NodeManager();
    
    void set_scope_manager(ScopeManager* sm);

    BlockParseNode *unbox(NodeIndex index);

    int get_node_count();

    string to_string(NodeIndex);
    void showtree(NodeIndex);
    ParseNodeType get_nodetype(NodeIndex);
    ParseNodeType get_host_nodetype(NodeIndex);
    string get_lexeme(NodeIndex);
    Token get_token(NodeIndex);
    string get_full_lexeme(NodeIndex);

    nodebox get_node(NodeIndex);
    NodeIndex get_left(NodeIndex);
    NodeIndex get_right(NodeIndex);
    NodeIndex get_middle(NodeIndex);
    NodeIndex get_branch(NodeIndex);
    NodeIndex reach_right(NodeIndex, int);
    vector<NodeIndex> get_children(NodeIndex);
    DependencyGraph *get_dependencies(NodeIndex);
    string get_identifier(NodeIndex);

    void set_children(NodeIndex, vector<NodeIndex>);

    void bind_dependency(NodeIndex, DependencyGraph*);
    void add_child(NodeIndex self, NodeIndex new_child);
    void delete_node(NodeIndex index);
    
    // Scope and symbol binding accessors
    void set_scope_id(NodeIndex node, int scope_id);
    int get_scope_id(NodeIndex node);
    void set_resolved_symbol(NodeIndex node, int symbol_index);
    int get_resolved_symbol(NodeIndex node);

    NodeIndex create(AbstractParseNode*, ParseNodeType);
    NodeIndex create_block();
    NodeIndex create_block(Token, ParseNodeType);
    NodeIndex create_block(Token, ParseNodeType, vector<NodeIndex>);

    NodeIndex create_binary(Token, ParseNodeType, NodeIndex, NodeIndex);

    NodeIndex create_trinary(Token, ParseNodeType, NodeIndex, NodeIndex, NodeIndex);

    NodeIndex create_unary(Token, ParseNodeType);
    NodeIndex create_unary(Token, ParseNodeType, NodeIndex branch);
    NodeIndex create_unary(Token, ParseNodeType, vector<string> lexemes);

    void expand_container();
    void check_capacity();
};
