//
// Created by alek on 1/3/26.
//
#pragma once

#include <map>
#include <set>
#include <string>

using namespace std;

class ScopeManager {
public:
    struct Scope {
        int id;
        int parent_id;           // -1 for global scope
        string name;             // "global", function name, etc.
        int start_line;
        int end_line;
        set<int> symbol_indices; // Symbols declared in this scope

        Scope();
        Scope(int id, int parent_id, const string& name, int start_line);
    };

    map<int, Scope> scopes;
    int current_scope_id;    // Currently active scope
    int next_scope_id;       // Counter for generating unique scope IDs

    ScopeManager();

    // Scope lifecycle
    int enter_scope(const string& name, int start_line);
    void exit_scope(int end_line);
    
    // Accessors
    int get_current_scope() const;
    int get_parent_scope(int scope_id) const;
    Scope* get_scope(int scope_id);
    const Scope* get_scope(int scope_id) const;
    
    // Scope hierarchy queries
    bool is_ancestor_of(int ancestor_id, int descendant_id) const;
    bool scope_exists(int scope_id) const;
    
    // Symbol registration within scopes
    void register_symbol_in_scope(int scope_id, int symbol_index);
    void register_symbol_in_current_scope(int symbol_index);
};
