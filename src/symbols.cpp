#include <string>
#include <set>
#include <stack>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/sage_types.h"

symbol_entry::symbol_entry() :
    value(SageValue()),
    type(nullptr),
    identifier(""),
    is_variable(false),
    is_parameter(false),
    spilled(false),
    assigned_register(-1),
    spill_offset(0){}

symbol_entry::symbol_entry(SageValue sval, string ident) :
    value(sval),
    type(sval.valuetype),
    identifier(ident),
    is_variable(false),
    is_parameter(false),
    spilled(false),
    assigned_register(-1),
    spill_offset(0){}


tablecell::tablecell():
    fulltable(nullptr),
    entries(vector<int>()),
    parent(nullptr),
    current_offset(0) {}

tablecell::tablecell(SageSymbolTable* root, tablecell* parent, int offset):
    fulltable(root),
    entries(vector<int>()),
    parent(parent),
    current_offset(offset) {}

int tablecell::declare_symbol(const string& name, SageValue value) {
    auto entries = fulltable->entries;
    int capacity = fulltable->capacity;

    entries[capacity].value = value;
    entries[capacity].type = value.valuetype;
    entries[capacity].identifier = name;
    entries[capacity].is_variable = true;
    entries[capacity].is_parameter = false;

    fulltable->capacity++;
    return capacity;
}

int tablecell::declare_symbol(const string& name, SageType* valuetype) {
    auto entries = fulltable->entries;
    int capacity = fulltable->capacity;

    entries[capacity].type = valuetype;
    entries[capacity].identifier = name;
    entries[capacity].is_variable = true;
    entries[capacity].is_parameter = false;

    fulltable->capacity++;
    return capacity;
}

int tablecell::declare_symbol(const string& name, int register_alloc) {
    auto entries = fulltable->entries;
    int capacity = fulltable->capacity;

    entries[capacity].type = nullptr;
    entries[capacity].assigned_register = register_alloc;
    entries[capacity].identifier = name;
    entries[capacity].is_variable = true;
    entries[capacity].is_parameter = false;

    fulltable->capacity++;
    return capacity;
}

int tablecell::lookup_idx(const string& name) {
    // double pointer search for name
    int b = entries.size()-1;
    for (int a = 0; a < entries.size(); ++a) {
        if (fulltable->entries[entries[a]].identifier == name) {
            return entries[a];
        }
        if (fulltable->entries[entries[b]].identifier == name) {
            return entries[b];
        }
        b++;
    }

    // otherwise continue search in parent
    if (parent == nullptr) {
        return -1;
    }

    return parent->lookup_idx(name);
}

symbol_entry* tablecell::lookup(const string& name) {
    // double pointer search for name
    int b = entries.size()-1;
    for (int a = 0; a < entries.size(); ++a) {
        if (fulltable->entries[entries[a]].identifier == name) {
            return &fulltable->entries[entries[a]];
        }
        if (fulltable->entries[entries[b]].identifier == name) {
            return &fulltable->entries[entries[b]];
        }
        b++;
    }

    // otherwise continue search in parent
    if (parent == nullptr) {
        return nullptr;
    }

    return parent->lookup(name);
}

symbol_entry* tablecell::lookup(int entry_index) {
    return &fulltable->entries[entry_index];
}

SageType* tablecell::resolve_sage_type(NodeManager* man, NodeIndex typenode) {
    string name = man->get_lexeme(typenode);

    // double pointer search for name
    int b = entries.size()-1;
    for (int a = 0; a < entries.size(); ++a) {
        if (fulltable->entries[entries[a]].identifier == name) {
            return fulltable->entries[entries[a]].type;
        }
        if (fulltable->entries[entries[b]].identifier == name) {
            return fulltable->entries[entries[b]].type;
        }
        b++;
    }

    // otherwise continue search in parent
    if (parent == nullptr) {
        return nullptr;
    }

    return parent->resolve_sage_type(man, typenode);
}








SageSymbolTable::SageSymbolTable() {
    this->function_visitor_state = stack<function_visit>();
    this->size = 0;
    this->capacity = 0;
    this->entries = nullptr;
    this->global_scope = nullptr;
    this->current = nullptr;
}

SageSymbolTable::SageSymbolTable(int size) {
    this->function_visitor_state = stack<function_visit>();
    this->size = size;
    this->capacity = 0;
    this->global_scope = new tablecell(this, nullptr, 0);
    this->current = global_scope;

    this->entries = new symbol_entry[size];
    for (int i = 0; i < size; ++i) {
        entries[i] = symbol_entry();
    }
}

SageSymbolTable::~SageSymbolTable(){}

void SageSymbolTable::initialize() {
    // initialize root stack with sage built in datatypes and methods
    auto chartype = TypeRegistery::get_builtin_type(CHAR);
    declare_type_symbol("bool", TypeRegistery::get_builtin_type(BOOL));
    declare_type_symbol("char",  chartype);
    declare_type_symbol("int", TypeRegistery::get_builtin_type(I64));
    declare_type_symbol("i8", TypeRegistery::get_builtin_type(I8));
    declare_type_symbol("i32", TypeRegistery::get_builtin_type(I32));
    declare_type_symbol("i64", TypeRegistery::get_builtin_type(I64));
    declare_type_symbol("float", TypeRegistery::get_builtin_type(F64));
    declare_type_symbol("f32", TypeRegistery::get_builtin_type(F32));
    declare_type_symbol("f64", TypeRegistery::get_builtin_type(F64));
    declare_type_symbol("void", TypeRegistery::get_builtin_type(VOID));

    // TODO FIX: this interpretation of pointer syntax does not account for the fact that there could be double or triple pointers
    declare_type_symbol("char*", TypeRegistery::get_pointer_type(chartype));

    // declare_symbol("executable_name", TypeRegistery::get_pointer_type(chartype));
    // declare_symbol("platform", TypeRegistery::get_pointer_type(chartype));
    // declare_symbol("architecture", TypeRegistery::get_pointer_type(chartype));
    // declare_symbol("bitsize", TypeRegistery::get_pointer_type(chartype));

    // SageScope root_scope = {
    // types = {
    //     "bool", "char", "int", "i8",
    //     "i32", "i64", "float", "f32", "f64", "void",
    //     "char*"
    // };
    // type_scope = root_scope;
}

void SageSymbolTable::enter_scope(int offset) {
    this->current = new tablecell(this, this->current, offset);
}

void SageSymbolTable::exit_scope() {
    if (this->current == nullptr) {
        return;
    }

    auto old_current = this->current;
    this->current = this->current->parent;
    delete old_current;
}

int SageSymbolTable::declare_symbol(const string& name, SageValue value) {
    return this->current->declare_symbol(name, value);
}

int SageSymbolTable::declare_symbol(const string& name, SageType* valuetype) {
    return this->current->declare_symbol(name, valuetype);
}

int SageSymbolTable::declare_symbol(const string& name, int register_alloc) {
    return this->current->declare_symbol(name, register_alloc);
}

int SageSymbolTable::lookup_idx(const string& name) {
    return this->current->lookup_idx(name);
}

symbol_entry* SageSymbolTable::lookup(const string& name) {
    return this->current->lookup(name);
}

symbol_entry* SageSymbolTable::lookup(int entry_index) {
    return this->current->lookup(entry_index);
}

void SageSymbolTable::declare_type_symbol(const string& name, SageType* type) {
    if (capacity >= size) { return; }

    types.insert(name);

    entries[capacity].type = type;
    entries[capacity].identifier = name;
    entries[capacity].is_variable = false;
    entries[capacity].is_parameter = false;

    capacity++;
}

SageType* SageSymbolTable::derive_sage_type(NodeManager* manager, NodeIndex typenode) {
    switch (manager->get_nodetype(typenode)) {
        case PN_NUMBER:
            return TypeRegistery::get_builtin_type(I64);
        case PN_FLOAT:
            return TypeRegistery::get_builtin_type(F64);
        case PN_STRING:
            return TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(CHAR));
        case PN_VAR_REF:
            // TODO
            break;
        default:
            break;
    }
    return TypeRegistery::get_builtin_type(VOID);
}

SageType* SageSymbolTable::resolve_sage_type(NodeManager* manager, NodeIndex typenode) {
    return this->current->resolve_sage_type(manager, typenode);
}





