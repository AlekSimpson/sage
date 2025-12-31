#pragma once

#include <string>
#include <vector>
#include <stack>
#include <set>

#include "node_manager.h"
#include "sage_types.h"

struct function_visit {
    string function_name = "";
    int return_statement_count = 0;

    function_visit(string name) : function_name(name) {};
    bool has_returned() const { return return_statement_count > 0; }
};

struct symbol_entry {
    SageValue value;
    SageType* type;
    string identifier;
    bool is_variable;
    bool is_parameter;
    bool spilled;
    int assigned_register;
    int spill_offset;

    symbol_entry();
    symbol_entry(SageValue, string);
};

class SageSymbolTable;

struct tablecell {
    SageSymbolTable* fulltable;
    vector<int> entries; // contains indices that address to fulltable->entries
    tablecell* parent;
    int current_offset;

    tablecell();
    tablecell(SageSymbolTable* root, tablecell* parent, int offset);

    int declare_symbol(const string& name, SageValue value);
    int declare_symbol(const string& name, SageType* valuetype);
    int declare_symbol(const string& name, int register_alloc);

    int lookup_idx(const string& name); // lookup identifier
    symbol_entry* lookup(const string& name);
    symbol_entry* lookup(int entry_index);
    SageType* resolve_sage_type(NodeManager*, NodeIndex);
};

class SageSymbolTable {
public:
    set<string> types;
    symbol_entry* entries;
    tablecell* global_scope;
    tablecell* current;
    stack<function_visit> function_visitor_state;
    vector<string*> string_pool;
    int size;
    int capacity;

    SageSymbolTable();
    SageSymbolTable(int size);
    ~SageSymbolTable();

    void enter_scope(int offset);
    void exit_scope();

    SageType* resolve_sage_type(NodeManager*, NodeIndex);
    SageType* derive_sage_type(NodeManager*, NodeIndex);

    void declare_type_symbol(const string& name, SageType* type);
    int declare_symbol(const string& name, SageValue value);
    int declare_symbol(const string& name, SageType* valuetype);
    int declare_symbol(const string& name, int register_alloc);

    int lookup_idx(const string& name); // lookup identifier
    symbol_entry* lookup(const string& name);
    symbol_entry* lookup(int entry_index);

    void initialize();
};













