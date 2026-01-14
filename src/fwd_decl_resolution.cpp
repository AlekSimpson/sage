#include <algorithm>
#include "../include/codegen.h"

using namespace std;

void SageCompiler::get_in_degree_of(
    const string& root_definition_identifier,
    NodeIndex current_node,
    int working_scope) {

    switch (node_manager->get_host_nodetype(current_node)) {
        case PN_UNARY: {
            auto nodetype = node_manager->get_nodetype(current_node);
            if (nodetype != PN_IDENTIFIER && nodetype != PN_VAR_REF) { return; }

            auto identifier = node_manager->get_identifier(current_node);
            if (symbol_table.lookup_from_scope(identifier, working_scope) == -1) { return; }
            if (previously_processed.find(identifier) != previously_processed.end()) { return; }

            // if the found reference is in scope of working scope
            // then add data dependency and increment in degree
            if (definition_dependencies.find(identifier) == definition_dependencies.end()) {
                definition_dependencies[identifier] = set<string>();
            }
            definition_dependencies[identifier].insert(root_definition_identifier);

            if (in_degree_map.find(root_definition_identifier) == in_degree_map.end()) {
                in_degree_map[root_definition_identifier] = 0;
            }
            in_degree_map[root_definition_identifier] += 1;
            break;
        }
        case PN_BINARY: {
            auto left = node_manager->get_left(current_node);
            auto right = node_manager->get_right(current_node);
            get_in_degree_of(root_definition_identifier, left, working_scope);
            get_in_degree_of(root_definition_identifier, right, working_scope);
            break;
        }
        case PN_TRINARY: {
            auto left = node_manager->get_left(current_node);
            auto middle = node_manager->get_middle(current_node);
            auto right = node_manager->get_right(current_node);
            get_in_degree_of(root_definition_identifier, left, working_scope);
            get_in_degree_of(root_definition_identifier, middle, working_scope);
            get_in_degree_of(root_definition_identifier, right, working_scope);
            break;
        }
        case PN_BLOCK: {
            for (auto child: node_manager->get_children(current_node)) {
                get_in_degree_of(root_definition_identifier, child, working_scope);
            }
            break;
        }
        default:
            break;
    }
}

void SageCompiler::resolve_definition_order(int target_scope) {
    vector<NodeIndex> result_order;
    stack<string> fringe;
    map<string, NodeIndex> identifier_to_ast;
    for (const auto& [identifier, in_degree]: in_degree_map) {
        auto ast_id = symbol_table.global_lookup(identifier)->definition_ast_index;
        identifier_to_ast[identifier] = ast_id;

        if (in_degree != 0) {continue;}
        fringe.push(identifier);
    }

    string current;
    set<string> visited;
    while (!fringe.empty()) {
        current = fringe.top();
        fringe.pop();

        if (visited.find(current) != visited.end()) {
            auto token = node_manager->get_token(identifier_to_ast[current]);
            logger.log_error(token, sen("Invalid redefinition of symbol:", current), SEMANTIC);
            break;
        }

        result_order.push_back(identifier_to_ast[current]);

        for (string child_dependency : definition_dependencies[current]) {
            in_degree_map[child_dependency] -= 1;
            if (in_degree_map[child_dependency] == 0) {
                fringe.push(child_dependency);
            }
        }
    }

    NodeIndex target_ast_root = scope_manager.scope_to_astroot[target_scope];

    // preserve order of non definition statements in scope while prepending new resolved defintion statement order
    for (auto child: node_manager->get_children(target_ast_root)) {
        auto nodetype = node_manager->get_nodetype(child);
        if (nodetype == PN_FUNCDEF || nodetype == PN_VAR_DEC || nodetype == PN_STRUCT) {
            continue;
        }

        result_order.push_back(child);
    }

    node_manager->set_children(target_ast_root, result_order);
}

void SageCompiler::forward_declaration_resolution(int program_root) {
    auto symbols_by_scope = symbol_table.symbols_sorted_by_scope_id(); // note: we can cache the output of this until program source changes are detected
    int current_scope = node_manager->get_scope_id(program_root);
    set<string> being_processed;
    in_degree_map.clear();
    previously_processed.clear();

    // for each scope, find every native definition and get its in_degree,
    // then sort those definitions into a valid compilation order
    //  *** in_degree represents amount of in sope references contained within the definition
    for (int symbol_id : symbols_by_scope) {
        auto ast_id = symbol_table.entries[symbol_id].definition_ast_index;

        if (current_scope != node_manager->get_scope_id(ast_id)) {
            resolve_definition_order(current_scope);
            current_scope = node_manager->get_scope_id(ast_id);

            previously_processed.insert(being_processed.begin(), being_processed.end());
            in_degree_map.clear();
            definition_dependencies.clear();
            being_processed.clear();
        }

        if (ast_id == -1) { continue; } // find all definitions in that scope

        switch (node_manager->get_nodetype(ast_id)) {
            case PN_FUNCDEF:
            case PN_VAR_DEC: {
                auto rhs = node_manager->get_right(ast_id);
                get_in_degree_of(node_manager->get_identifier(ast_id), rhs, current_scope);
                break;
            }
            case PN_STRUCT: {
                auto body = node_manager->get_branch(ast_id);
                get_in_degree_of(node_manager->get_identifier(ast_id), body, current_scope);
                break;
            }
            default:
                break;
        }
    }
}