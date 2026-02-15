#pragma once

#include <string>
#include <stack>
#include <set>
#include <map>

#include "comptime_manager.h"
#include "scope_manager.h"
#include "node_manager.h"
#include "sage_types.h"
#include "sage_value.h"

#define SAGE_NULL_SYMBOL 0

#define NULL_SYMBOL_NAME "__SAGE_NULL_SYMBOL__"
#define GLOBAL_NAME "___SAGE__GLOBAL__FUNCTION___3827"

struct FieldMember {
    string name;
    string type_identifier;

    struct FieldMemberHash {
        size_t operator()(const FieldMember& member) const {
            size_t hash1 = std::hash<string>{}(member.name);
            size_t hash2 = std::hash<string>{}(member.type_identifier);
            return hash1 ^ (hash2 << 1);
        }
    };
};

struct SageNamespace {
    unordered_map<FieldMember, int, FieldMember::FieldMemberHash> field_member_offsets;
    unordered_map<FieldMember, SymbolIndex, FieldMember::FieldMemberHash> struct_methods;

    bool field_is_a_method(string function_name, string function_type_string);
    bool field_is_a_member(string search_name, string search_type_identifier);

    void add_method(string search, string function_type);
    void add_field_member(string name, string type_identifier);

    void inject_namespace(SageNamespace &other);
};

struct SymbolEntry {
    SageNamespace *type_namespace = nullptr;
    string name = "";
    SageValue data = SageValue();
    SageType *datatype = nullptr;
    NodeIndex definition_ast_index = -1;
    int scope_id = -1;
    int symbol_index = -1;

    int return_statement_count = 0;
    int stack_return_pointer_counter = 0;
    int max_return_count = 0;

    bool spilled = false;
    int assigned_register = -1;
    int stack_offset = 0;
    int static_stack_pointer = -1;

    ComptimeTaskId task_id;

    SymbolEntry()  {};
    ~SymbolEntry() {
        if (type_namespace != nullptr) {
            delete type_namespace;
        }
    };
    SymbolEntry(SageValue value, string identifier) : name(identifier), data(value) {}

    SageNamespace *get_namespace();
    bool has_returned() const { return return_statement_count > 0; }
    bool type_is_resolved();
    bool needs_comptime_resolution();
    void spill(int offset);
};

class SymbolArena {
public:
    SymbolEntry *data;
    int CAPACITY;
    int size = 0;
    bool is_empty = true;

    SymbolArena(): data(nullptr), CAPACITY(0) {}
    SymbolArena(int capacity): CAPACITY(capacity) {
        data = new SymbolEntry[CAPACITY];
    }

    ~SymbolArena() {
        if (data != nullptr) {
            delete[] data;
        }
    }

    SymbolArena(SymbolArena &&other) noexcept
        : data(other.data), CAPACITY(other.CAPACITY),
          size(other.size), is_empty(other.is_empty) {
        other.data = nullptr;
        other.size = 0;
        other.is_empty = true;
    }

    SymbolArena &operator=(SymbolArena &&other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            CAPACITY = other.CAPACITY;
            size = other.size;
            is_empty = other.is_empty;
            other.data = nullptr;
            other.size = 0;
            other.is_empty = true;
        }
        return *this;
    }

    SymbolArena(const SymbolArena &) = delete;
    SymbolArena &operator=(const SymbolArena &) = delete;

    SymbolIndex allocate_symbol();
    SymbolEntry &get(SymbolIndex index);
    SymbolEntry *get_pointer(SymbolIndex index);
};

class SageSymbolTable {
public:
    SymbolArena entries;
    NodeManager *nm;
    ScopeManager *scope_manager;
    set<string> identifiers_that_must_be_spilled;
    stack<SymbolIndex> function_processing_context;
    set<SymbolIndex> builtins;
    set<SymbolIndex> variables;
    set<SymbolIndex> types;
    set<SymbolIndex> functions;
    set<SymbolIndex> comptime_values;

    // (scope_id, name) -> entry index for fast lookup
    map<pair<int, string>, SymbolIndex> scope_symbol_map;
    map<ComptimeTaskId, SymbolIndex> comptime_task_id_to_symbol_id;
    map<string, SageType *> cached_type_identifiers;

    int temporary_counter_gen = 0;
    bool program_uses_main_function = false;

    SageSymbolTable();
    SageSymbolTable(ScopeManager* scopeman, NodeManager *nm, int size);

    void push_function_processing_context(SymbolIndex new_processing_context);
    void pop_function_processing_context();
    SymbolEntry &function_being_processed();

    bool is_comptime_value(SymbolEntry *);

    SageType *resolve_type_identifier(string, int);
    SageType *resolve_variable_type(SymbolIndex);
    SageType *resolve_function_type(SymbolIndex);
    SageType *resolve_struct_type(SymbolIndex);

    // Symbol declarations
    void declare_null_symbol();
    void declare_builtin_type_symbol(const string &name, SageType *type);
    SymbolIndex declare_builtin_function(const string &name, SageType *type);

    SymbolIndex declare_literal(NodeIndex ast_id, SageValue value);
    SymbolIndex declare_variable(NodeIndex ast_id, SageType *valuetype);
    SymbolIndex declare_parameter(NodeIndex ast_id, SageType *valuetype, int parameter_register_assignment);
    SymbolIndex declare_type_symbol(NodeIndex ast_id, SageType *type);
    SymbolIndex declare_function(NodeIndex ast_id, SageType *type); // creates FunctionVisit value, lives here now

    // Lookups
    const SymbolEntry *global_lookup(const string &name);
    SymbolEntry *lookup_by_index(SymbolIndex entry_index);
    SymbolEntry *lookup(const string &name, int scope_id);
    SymbolIndex lookup_table_index(const string &name, int scope_id);
    
    // Check if symbol is visible from a given scope
    bool is_visible(SymbolIndex symbol_index, int from_scope_id);
    void register_comptime_value(ComptimeManager &, NodeIndex, SymbolIndex);
    int get_result_total_byte_size(SymbolIndex);
    int get_amount_of_elements_being_returned(SymbolIndex);
    bool needs_return_stack_pointer(SymbolIndex);
    void initialize();
};
