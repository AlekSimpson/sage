#include <stdlib.h>
#include <stack>
#include <queue>
#include <boost/algorithm/string.hpp>
#include <numeric>

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"
#include "../include/scope_manager.h"

using namespace std;

int ast_bookmark_sorter(comptime_ast_bookmark mark) {
    return mark.scope_level;
}

SageCompiler::SageCompiler() {
}

SageCompiler::SageCompiler(string mainfile)
    : ast(NULL_INDEX),
      debug(COMPILATION),
      node_manager(new NodeManager()),
      scope_manager(ScopeManager()),
      parser(SageParser(&scope_manager, node_manager, mainfile)),
      interpreter(new SageInterpreter(&symbol_table, 4046)),
      builder(BytecodeBuilder()) {
    // Set scope_manager on node_manager for automatic scope_id assignment
    node_manager->set_scope_manager(&scope_manager);

    volatile_register_state = {
        {10, SageValue()},
        {11, SageValue()},
        {12, SageValue()},
        {13, SageValue()},
        {14, SageValue()},
        {15, SageValue()},
        {16, SageValue()},
        {17, SageValue()},
        {18, SageValue()},
        {19, SageValue()},
        {20, SageValue()},
    };
}

SageCompiler::~SageCompiler() {
    delete node_manager;
    delete interpreter;
}

void SageCompiler::print_bytecode(bytecode &code) {
    int count = 0;
    for (const auto &[id, frame]: builder.procedures) {
        printf("%s: %d\n", frame.name.c_str(), id);
    }
    printf("------------\n");
    for (auto instruction: code) {
        printf("%d: %s\n", count, instruction.print().c_str());
        count++;
    }
}

NodeIndex SageCompiler::parse_codefile(string target_file) {
    parser.filename = target_file;
    NodeIndex parsetree = parser.parse_program(false);
    if (parsetree == NULL_INDEX) {
        logger.log_internal_error("compiler.cpp", current_linenum, "AST root is null. parsing failed.");
        return NULL_INDEX;
    }
    return parsetree;
}

bytecode SageCompiler::compile(NodeIndex ast_index) {
    if (ast_index == NULL_INDEX) {
        return bytecode();
    }

    // if (compiling_root) {
    //     compile_dependency_resolution_order(node_manager->get_dependencies(ast_index));
    // }
    visit(ast_index);
    bytecode output = builder.final(interpreter->proc_line_locations, &symbol_table, generate_compile_time_bytecode);

    printf("======================\n");
    print_bytecode(output);
    printf("======================\n");
    builder.reset();

    return output;
}

void SageCompiler::begin_compilation(string mainfile) {
    if (!check_filename_valid(mainfile)) {
        logger.log_error(mainfile, -1, "Main program filename is not valid. Make sure it ends in '.sage'", GENERAL);
        return;
    }

    NodeIndex ast_root = parser.parse_program(debug == PARSING || debug == ALL);
    if (ast_root == NULL_INDEX) {
        logger.log_internal_error("compiler.cpp", current_linenum, "AST root was null. Parsing failed.");
        return;
    }
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    int symbol_count = BUILTIN_COUNT + parser.symbol_count;
    symbol_table = SageSymbolTable(&scope_manager, node_manager, symbol_count);
    symbol_table.initialize();

    scan_all_program_symbols(ast_root);

    perform_type_resolution();
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // auto resolve symbol definition ordering
    forward_declaration_resolution(ast_root);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    register_allocation();
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // execute run directives
    generate_compile_time_bytecode = true;
    comptime_ast_bookmark mark;
    string rundir_name;
    bookmarked_run_directives = ascending_list<comptime_ast_bookmark>(node_manager->get_node_count());
    for (int i = 0; i < bookmarked_run_directives.size; ++i) {
        mark = bookmarked_run_directives[i];
        rundir_name = str("rundir", i);
        builder.procedures[hash_djb2(rundir_name)] = procedure_frame(rundir_name);
        builder.procedure_stack.push(hash_djb2(rundir_name));
        bytecode code = compile(mark.ast_position);
        if (code.empty()) {
            continue;
        }
        precompiled.insert(mark.ast_position);
        interpreter->load_program(code);
        interpreter->execute();
    }
    generate_compile_time_bytecode = false;

    // final program compilation pass
    // compile runtime code
    bytecode code = compile(ast_root);

    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    ///////////////
    interpreter->load_program(code);
    interpreter->execute();
    interpreter->close();
    // ^^^^^^^^TEMP!!!

    // 3. Compiling to target instructions (if sagevm not the target)

    // 4. Linking
    /*bool success = emit_and_link_llvm(module, "sage.out"); */

    // if (success) {
    //     printf("Compilation finished successfully.\n");
    // }else {
    //     printf("Compilation finished unsuccessfully.\n");
    // }
}

void SageCompiler::scan_all_program_symbols(NodeIndex root) {
    NodeIndex current_node;

    map<string, int> function_max_parameters_mapping;
    map<string, int> function_parameter_register_generator;
    map<string, string> parameter_name_to_parent_function;
    set<string> scanned_literals;

    queue<NodeIndex> fringe;
    fringe.push(root);

    // scan for all symbols
    while (!fringe.empty()) {
        current_node = fringe.front();
        fringe.pop();

        auto nodetype = node_manager->get_nodetype(current_node);
        switch (nodetype) {
            case PN_STRUCT: {
                table_index table_idx = symbol_table.declare_type_symbol(current_node, nullptr);
                symbol_table.structs.insert(table_idx);

                auto bodynode = node_manager->reach_right(current_node, 2);
                for (auto child: node_manager->get_children(bodynode)) {
                    fringe.push(child);
                }
                continue;
            }
            case PN_FUNCDEF: {
                auto table_idx = symbol_table.declare_symbol(current_node, nullptr);
                symbol_table.functions.insert(table_idx);
                string identifier = node_manager->get_identifier(current_node);

                auto signature_trinary = node_manager->get_right(current_node);

                auto paramnode = node_manager->get_left(signature_trinary);
                auto bodynode = node_manager->reach_right(current_node, 2);
                function_max_parameters_mapping[identifier] = node_manager->get_children(paramnode).size();
                function_parameter_register_generator[identifier] = 0;
                string child_ident;
                for (auto child: node_manager->get_children(paramnode)) {
                    child_ident = node_manager->get_identifier(child);
                    fringe.push(child);
                    parameter_name_to_parent_function[child_ident] = identifier;
                }
                fringe.push(node_manager->get_middle(signature_trinary)); // scan the return type
                for (auto child: node_manager->get_children(bodynode)) {
                    fringe.push(child);
                }
                continue;
            }
            case PN_VAR_DEC: {
                auto identifier = node_manager->get_identifier(current_node);
                if (parameter_name_to_parent_function.find(identifier) != parameter_name_to_parent_function.end()) {
                    string parent_function_name = parameter_name_to_parent_function[identifier];
                    symbol_table.declare_parameter(current_node, nullptr, function_parameter_register_generator[parent_function_name]);
                    function_parameter_register_generator[parent_function_name]++;
                }else {
                    symbol_table.declare_variable(current_node, nullptr);
                }
                fringe.push(node_manager->get_right(current_node));
                continue;
            }
            case PN_FOR:
            case PN_IF:
            case PN_IF_BRANCH:
            case PN_ELSE_BRANCH:
            case PN_WHILE: {
                auto bodynode = node_manager->get_right(current_node);
                for (auto child: node_manager->get_children(bodynode)) {
                    fringe.push(child);
                }
                continue;
            }
            case PN_RUN_DIRECTIVE: {
                auto bodynode = node_manager->get_branch(current_node);
                for (auto child: node_manager->get_children(bodynode)) {
                    fringe.push(child);
                }
                continue;
            }
            case PN_BLOCK: {
                for (auto child: node_manager->get_children(current_node)) {
                    fringe.push(child);
                }
                continue;
            }
            case PN_TRINARY:
            case PN_BINARY: {
                fringe.push(node_manager->get_right(current_node));
                continue;
            }
            case PN_FUNCCALL: {
                fringe.push(node_manager->get_branch(current_node));
                continue;
            }
            case PN_KEYWORD: {
                if (node_manager->get_branch(current_node) == NULL_INDEX) continue;
                fringe.push(node_manager->get_branch(current_node));
                continue;
            }

            case PN_STRING: {
                auto node_lexeme = node_manager->get_lexeme(current_node);
                node_lexeme.erase(std::remove(node_lexeme.begin(), node_lexeme.end(), '"'), node_lexeme.end());
                if (scanned_literals.find(node_lexeme) != scanned_literals.end()) continue;
                scanned_literals.insert(node_lexeme);

                process_escape_sequences(node_lexeme);
                auto *char_type = TypeRegistery::get_byte_type(CHAR);
                auto *array_type =  TypeRegistery::get_array_type(char_type, node_lexeme.length());
                SageValue literal_value(const_cast<void *>(static_cast<const void *>(node_lexeme.c_str())), array_type);
                symbol_table.declare_literal(current_node, literal_value);
                continue;
            }
            default:
                continue;
        }
    }

    return;
}

void SageCompiler::perform_type_resolution() {
    vector<table_index> program_symbols(symbol_table.entries.size());
    std::iota(program_symbols.begin(), program_symbols.end(), 0);

    for (table_index idx: program_symbols) {
        if (symbol_table.builtins.find(idx) != symbol_table.builtins.end()) continue;
        if (symbol_table.entries[idx].type_is_resolved()) continue;

        auto nodetype = node_manager->get_nodetype(symbol_table.entries[idx].definition_ast_index);
        switch (nodetype) {
            case PN_VAR_DEC:
                symbol_table.entries[idx].type = symbol_table.resolve_variable_type(idx);
                break;
            case PN_STRUCT:
                symbol_table.entries[idx].type = symbol_table.resolve_struct_type(idx);
                break;
            case PN_FUNCDEF:
                symbol_table.entries[idx].type = symbol_table.resolve_function_type(idx);
                break;
            default:
                logger.log_internal_error(
                    "compiler.cpp",
                    current_linenum,
                    "Type resolution encountered unknown symbol type");
                break;
        }
    }
}

bool SageCompiler::check_filename_valid(const string &filename) {
    // validate that file is valid
    if (filename.find('.') == string::npos) {
        logger.log_error(filename, -1, "cannot target files that have no file extension.", GENERAL);
        return false;
    }

    vector<string> delimited;
    boost::split(delimited, filename, boost::is_any_of("."));

    if (delimited[1] != "sage") {
        logger.log_error(filename, -1, "cannot target non-sage source files.", GENERAL);
        return false;
    }

    return true;
}

void SageCompiler::register_allocation() {
    /*
     *  Factors to consider for variable-register mapping sorting order
     *  1. Usage amount throughout scope
     *  2. Local to scope means higher priority
     *  3. Is it used in a loop?
     *
     *  for each scope:
     *  1. expire variables that are out of scope
     *  2. allocate free registers to variables declared in current scope (allocations written to symbol table)
     */

    //auto symbols_by_scope = symbol_table.variables_sorted_by_scope_id();
    vector<table_index> variables_by_scope(symbol_table.variables.begin(), symbol_table.variables.end());
    variables_by_scope.insert(variables_by_scope.end(), symbol_table.constants.begin(), symbol_table.constants.end());

    // then arange what is left according to scope_id descending
    std::sort(variables_by_scope.begin(), variables_by_scope.end(), [this](int a, int b) {
        return (symbol_table.entries[a].scope_id < symbol_table.entries[b].scope_id);
    });

    set<int> available_registers;
    set<string> working_symbols;
    int current_scope = 0;
    int current_relative_stack_location = 0;
    for (int r = GENERAL_REG_RANGE_BEGIN; r < GENERAL_REG_RANGE_END; ++r) {
        available_registers.insert(r);
    }

    symbol_entry *entry;
    int assigned_register;
    for (table_index idx: variables_by_scope) {
        auto current_ident = symbol_table.entries[idx].identifier;
        if (current_scope != symbol_table.entries[idx].scope_id) {
            current_relative_stack_location = 0;
            current_scope = symbol_table.entries[idx].scope_id;
        }

        for (auto symbol: working_symbols) {
            entry = symbol_table.lookup(symbol, current_scope);
            if (entry != nullptr) continue;

            working_symbols.erase(symbol);
            available_registers.insert(symbol_table.entries[current_scope].assigned_register);
        }

        if (available_registers.empty()) {
            // no room -> spill
            symbol_table.entries[idx].spill(current_relative_stack_location);
            current_relative_stack_location++;
            continue;
        }

        assigned_register = *available_registers.begin();
        symbol_table.entries[idx].assigned_register = assigned_register;
        working_symbols.insert(symbol_table.entries[idx].identifier);
        available_registers.erase(assigned_register);
    }
}

bool SageCompiler::node_is_precompiled(NodeIndex node) {
    return precompiled.find(node) != precompiled.end();
}

int SageCompiler::get_volatile() {
    int retval = 10 + (volatile_index % volatile_register_state.size());
    volatile_index++;
    return retval;
}

bool SageCompiler::volatile_is_stale(SageValue &live, int volatile_idx) {
    return !volatile_register_state[volatile_idx].equals(live);
}


void SageCompiler::get_in_degree_of(
    const string &root_definition_identifier,
    NodeIndex current_node,
    int working_scope) {
    switch (node_manager->get_host_nodetype(current_node)) {
        case PN_UNARY: {
            auto nodetype = node_manager->get_nodetype(current_node);
            if (nodetype != PN_IDENTIFIER && nodetype != PN_VAR_REF && nodetype != PN_TYPE && nodetype != PN_FUNCCALL) {
                return;
            }

            auto identifier = node_manager->get_identifier(current_node);
            auto symbol = symbol_table.lookup(identifier, working_scope);
            if (symbol == nullptr) { return; }
            if (previously_processed.find(identifier) != previously_processed.end()) { return; }
            if (symbol_table.builtins.find(symbol->symbol_id) != symbol_table.builtins.end()) { return; }
            if (symbol->definition_ast_index == -1) {
                Token found_tok = node_manager->get_token(current_node);
                logger.log_error(found_tok, sen("Undefined reference:", identifier), SEMANTIC);
                return;
            }

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
    int sum_of_in_degrees = 0;
    for (const auto &[identifier, in_degree]: in_degree_map) {
        auto ast_id = symbol_table.global_lookup(identifier)->definition_ast_index;
        identifier_to_ast[identifier] = ast_id;
        sum_of_in_degrees += in_degree;

        if (in_degree != 0) { continue; }
        fringe.push(identifier);
    }

    if (sum_of_in_degrees == 0) {
        return;
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

        for (string child_dependency: definition_dependencies[current]) {
            in_degree_map[child_dependency] -= 1;
            if (in_degree_map[child_dependency] == 0) {
                fringe.push(child_dependency);
            }
        }
    }

    if (result_order.empty()) { return; }

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
    vector<table_index> definitions_by_scope(symbol_table.variables.begin(), symbol_table.variables.end());
    definitions_by_scope.insert(definitions_by_scope.end(), symbol_table.structs.begin(), symbol_table.structs.end());
    definitions_by_scope.insert(definitions_by_scope.end(), symbol_table.functions.begin(), symbol_table.functions.end());
    // then arange what is left according to scope_id descending
    std::sort(definitions_by_scope.begin(), definitions_by_scope.end(), [this](int a, int b) {
        return (symbol_table.entries[a].scope_id < symbol_table.entries[b].scope_id);
    });
    definitions_by_scope.push_back(SAGE_NULL_SYMBOL);

    int current_scope = node_manager->get_scope_id(program_root);
    in_degree_map.clear();
    previously_processed.clear();

    // for each scope, find every native definition and get its in_degree,
    // then sort those definitions into a valid compilation order
    //  *** in_degree represents amount of in sope references contained within the definition
    for (int symbol_id: definitions_by_scope) {
        if (current_scope != symbol_table.entries[symbol_id].scope_id) {
            resolve_definition_order(current_scope);
            if (symbol_id == SAGE_NULL_SYMBOL) break;

            current_scope = symbol_table.entries[symbol_id].scope_id;

            for (const auto &[identifier, in_degree]: in_degree_map) {
                previously_processed.insert(identifier);
            }
            in_degree_map.clear();
            definition_dependencies.clear();
        }

        auto ast_id = symbol_table.entries[symbol_id].definition_ast_index;
        if (ast_id == -1) continue; // find all definitions in that scope

        in_degree_map[node_manager->get_identifier(ast_id)] = 0;
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
