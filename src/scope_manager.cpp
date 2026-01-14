//
// Created by alek on 1/3/26.
//

#include "../include/node_manager.h"
#include "../include/scope_manager.h"

// Scope struct constructors
ScopeManager::Scope::Scope() 
    : id(-1), parent_id(-1), name(""), start_line(-1), end_line(-1) {}

ScopeManager::Scope::Scope(int id, int parent_id, const string& name, int start_line)
    : id(id), parent_id(parent_id), name(name), start_line(start_line), end_line(-1) {}

// ScopeManager constructor
ScopeManager::ScopeManager() : current_scope_id(0), next_scope_id(1) {
    // Create global scope (scope 0)
    scopes[0] = Scope(0, -1, "global", 0);
}

int ScopeManager::enter_scope(const string& name, int start_line) {
    int new_scope_id = next_scope_id++;
    scopes[new_scope_id] = Scope(new_scope_id, current_scope_id, name, start_line);
    current_scope_id = new_scope_id;
    return new_scope_id;
}

void ScopeManager::exit_scope(int end_line, int bodynode) {
    if (current_scope_id == 0) {
        // Cannot exit global scope
        return;
    }
    
    scope_to_astroot[current_scope_id] = bodynode;
    scopes[current_scope_id].end_line = end_line;
    current_scope_id = scopes[current_scope_id].parent_id;
}

int ScopeManager::get_current_scope() const {
    return current_scope_id;
}

int ScopeManager::get_parent_scope(int scope_id) const {
    auto it = scopes.find(scope_id);
    if (it != scopes.end()) {
        return it->second.parent_id;
    }
    return -1;
}

ScopeManager::Scope* ScopeManager::get_scope(int scope_id) {
    auto it = scopes.find(scope_id);
    if (it != scopes.end()) {
        return &it->second;
    }
    return nullptr;
}

const ScopeManager::Scope* ScopeManager::get_scope(int scope_id) const {
    auto it = scopes.find(scope_id);
    if (it != scopes.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ScopeManager::is_ancestor_of(int ancestor_id, int descendant_id) const {
    if (ancestor_id == descendant_id) {
        return true;
    }
    
    int current = descendant_id;
    while (current != -1) {
        if (current == ancestor_id) {
            return true;
        }
        current = get_parent_scope(current);
    }
    return false;
}

bool ScopeManager::scope_exists(int scope_id) const {
    return scopes.find(scope_id) != scopes.end();
}

set<string> ScopeManager::in_scope_identifiers(NodeManager* nm, int scope_id) {
    auto symbol_indices = scopes[scope_id].symbol_indices;
    set<string> in_scope_identifiers;
    for (auto index: symbol_indices) {
        in_scope_identifiers.insert(nm->get_identifier(index));
    }
    return in_scope_identifiers;
}

void ScopeManager::register_symbol_in_scope(int scope_id, int symbol_index) {
    auto it = scopes.find(scope_id);
    if (it != scopes.end()) {
        it->second.symbol_indices.insert(symbol_index);
    }
}

void ScopeManager::register_symbol_in_current_scope(int symbol_index) {
    register_symbol_in_scope(current_scope_id, symbol_index);
}
