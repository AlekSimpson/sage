#pragma once

#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>

#include "scope_manager.h"
#include "node_manager.h"
#include "sage_types.h"

#define SAGE_NULL_SYMBOL 0

struct function_visit {
    string function_name = "";
    int return_statement_count = 0;

    function_visit(string name) : function_name(name) {};
    bool has_returned() const { return return_statement_count > 0; }
};

struct symbol_entry {
    SageValue value;
    SageType *type;
    string identifier;
    int assigned_register;
    int spill_offset;
    int constant_pool_index; // Index into constant pool (valid when in_constant_pool is true)
    NodeIndex definition_ast_index = -1;
    int scope_id;            // Scope where symbol was declared
    int symbol_id;           // Unique ID for this symbol
    bool is_variable;
    bool is_parameter;
    bool spilled;
    bool in_constant_pool;   // True if value is stored in constant pool (for 64-bit values like pointers)

    symbol_entry();
    symbol_entry(SageValue, string);
};

class SageSymbolTable {
public:
    set<string> types;
    vector<symbol_entry> entries;
	ScopeManager *scope_manager;
    stack<function_visit> function_visitor_state;
    vector<string*> string_pool;
    
    // (scope_id, name) -> entry index for fast lookup
    map<pair<int, string>, int> scope_symbol_map;
    
    int size;
    int capacity;

    SageSymbolTable();
    SageSymbolTable(ScopeManager* scopeman, int size);
    ~SageSymbolTable();

    SageType *resolve_sage_type(NodeManager*, NodeIndex);
    SageType *derive_sage_type(NodeManager*, NodeIndex);

    vector<int> symbols_sorted_by_scope_id();

    // Symbol declarations
    void declare_NULL_symbol();
    void declare_type_symbol(const string &name, SageType *type);
    int declare_symbol(const string &name, SageValue value);
    int declare_symbol(const string &name, SageType *valuetype);
    int declare_symbol(const string &name, int register_alloc);
    int declare_symbol_in_scope(const string &name, SageType *valuetype, NodeIndex ast_id, int scope_id);

    // Lookup by name
    int lookup_idx(const string &name);
    symbol_entry *lookup(const string &name); // legacy - uses current scope
    const symbol_entry *global_lookup(const string &name);
    
    // Direct access by resolved index (fast path)
    symbol_entry *lookup_by_index(int entry_index);
    
    // Lookup starting from given scope, chaining to parents
    int lookup_from_scope(const string &name, int scope_id);
    
    // Check if symbol is visible from a given scope
    bool is_visible(int symbol_index, int from_scope_id);

    void initialize();
};
