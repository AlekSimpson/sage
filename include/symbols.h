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

typedef uint32_t table_index;

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
    NodeIndex definition_ast_index = -1;
    int scope_id;
    table_index symbol_id;
    bool spilled;
    bool is_comptime_constant = false;

    symbol_entry();
    symbol_entry(SageValue, string);
    bool type_is_resolved();
    void spill(int offset);
    bool needs_comptime_resolution();
};

class SageSymbolTable {
public:
    vector<symbol_entry> entries; // TODO: make symbol table use arena allocator instead of vector
    NodeManager *nm;
	ScopeManager *scope_manager;
    stack<function_visit> function_visitor_state;
    set<table_index> builtins;
    set<table_index> variables;
    set<table_index> structs;
    set<table_index> functions;
    set<table_index> parameters;
    set<table_index> constants;
    set<table_index> literals; // used for strings and big literal values
    set<table_index> types;

    // (scope_id, name) -> entry index for fast lookup
    map<pair<int, string>, table_index> scope_symbol_map;

    int size; // MAX SIZE THE TABLE CAN HOLD
    int capacity; // the current capacity of the table
    int temporary_counter_gen = 0;

    SageSymbolTable();
    SageSymbolTable(ScopeManager* scopeman, NodeManager *nm, int size);
    ~SageSymbolTable();

    SageType *resolve_variable_type(table_index);
    SageType *resolve_function_type(table_index);
    SageType *resolve_struct_type(table_index);

    bool is_variable(table_index idx) { return variables.find(idx) != variables.end(); }
    bool is_parameter(table_index idx) { return parameters.find(idx) != parameters.end(); }
    bool is_constant(table_index idx) { return constants.find(idx) != constants.end(); }
    bool is_literal(table_index idx) { return literals.find(idx) != literals.end(); }
    bool is_type(table_index idx) { return types.find(idx) != types.end(); }

    // Symbol declarations
    void declare_builtin_type_symbol(const string &name, SageType *type);
    void declare_builtin_symbol(const string &name, SageType *type);

    table_index declare_immediate(SageValue value, string lexeme);
    table_index declare_literal(NodeIndex ast_id, SageValue value);
    table_index declare_constant(NodeIndex ast_id, SageValue value);
    table_index declare_temporary(int register_alloc);
    table_index declare_symbol(NodeIndex ast_id, SageType *valuetype);
    table_index declare_variable(NodeIndex ast_id, SageType *valuetype);
    table_index declare_parameter(NodeIndex ast_id, SageType *valuetype, int parameter_register_assignment);
    table_index declare_type_symbol(NodeIndex ast_id, SageType *type);
    table_index declare_comptime_result(const string &name, int scope_id, SageValue value);

    // Lookups
    const symbol_entry *global_lookup(const string &name);
    symbol_entry *lookup_by_index(table_index entry_index);
    symbol_entry *lookup(const string &name, int scope_id);
    table_index lookup_table_idx(const string &name, int scope_id);
    
    // Check if symbol is visible from a given scope
    bool is_visible(table_index symbol_index, int from_scope_id);

    void initialize();
};
