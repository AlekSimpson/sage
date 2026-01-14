#include <stdlib.h>
#include <stack>
#include <queue>
#include <boost/algorithm/string.hpp>
#include <sys/syscall.h>

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

SageCompiler::SageCompiler() {}

SageCompiler::SageCompiler(string mainfile)
    : ast(NULL_INDEX),
      debug(COMPILATION),
      node_manager(new NodeManager()),
      scope_manager(ScopeManager()),
      parser(SageParser(&scope_manager, node_manager, mainfile)),
      symbol_table(SageSymbolTable()),
      interpreter(nullptr),
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

void SageCompiler::print_bytecode(bytecode& code) {
    int count = 0;
    for (string name : builder.procs) {
        printf("%s: %d\n", name.c_str(), hash_djb2(name));
    }
    printf("------------\n");
    for (auto instruction : code) {
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
    bytecode output = builder.final(interpreter->proc_line_locations, interpreter_mode);
    
    // Transfer constant pool to interpreter (contains 64-bit values like pointers)
    interpreter->load_constant_pool(builder.get_constant_pool());
    
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
    if (ast_root == -1) {
        logger.log_internal_error("compiler.cpp", current_linenum, "AST root was null. Parsing failed.");
        return;
    }
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    int symbol_count = BUILTIN_COUNT + parser.symbol_count;
    symbol_table = SageSymbolTable(&scope_manager, symbol_count);
    symbol_table.initialize();

    // setup builtin functions
    vector<SageType*> puti_params = {
        TypeRegistery::get_builtin_type(I64),
        TypeRegistery::get_builtin_type(I64)
    };
    vector<SageType*> puts_params = {
        TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(CHAR)),
        TypeRegistery::get_builtin_type(I64)
    };
    symbol_table.declare_symbol_in_scope("puti", TypeRegistery::get_function_type(puti_params, vector<SageType*>()), ast_root, 0);
    symbol_table.declare_symbol_in_scope("puts", TypeRegistery::get_function_type(puts_params, vector<SageType*>()), ast_root, 0);

    // First AST pass
    // - creates all symbols
    // - TODO: does type checking
    perform_first_compilation_pass(ast_root);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // second pass auto resolves symbol definition ordering
    forward_declaration_resolution(ast_root);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // Register Allocation
    // register_allocation(dep_graph);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // 2. program compilation
    interpreter = new SageInterpreter(4046);

    // execute run directives
    interpreter_mode = true;
    comptime_ast_bookmark mark;
    string rundir_name;
    bookmarked_run_directives = ascending_list<comptime_ast_bookmark>(node_manager->get_node_count());
    for (int i = 0; i < bookmarked_run_directives.size; ++i) {
        mark = bookmarked_run_directives[i];
        rundir_name = str("rundir", i);
        builder.procedures[hash_djb2(rundir_name)] = procedure_frame(rundir_name);
        builder.procedure_stack.push(hash_djb2(rundir_name));
        builder.procs.push_back(rundir_name);
        bytecode code = compile(mark.ast_position);
        if (code.empty()) {
            continue;
        }
        precompiled.insert(mark.ast_position);
        interpreter->load_program(code);
        interpreter->execute();
    }
    interpreter_mode = false;

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

void SageCompiler::perform_first_compilation_pass(NodeIndex root) {
    NodeIndex current_node;

    queue<NodeIndex> fringe;
    fringe.push(root);

    // scan for all symbols
    while (!fringe.empty()) {
        current_node = fringe.front();
        fringe.pop();

        auto nodetype = node_manager->get_nodetype(current_node);
        switch (nodetype) {
            case PN_STRUCT:
            case PN_FUNCDEF: {
                string identifier = node_manager->get_identifier(current_node);
                symbol_table.declare_symbol_in_scope(identifier, nullptr, current_node, node_manager->get_scope_id(current_node));

                auto bodynode = node_manager->get_right(current_node);
                for (auto child: node_manager->get_children(bodynode)) {
                    fringe.push(child);
                }
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


            case PN_VAR_DEC: {
                string identifier = node_manager->get_identifier(current_node);
                symbol_table.declare_symbol_in_scope(identifier, nullptr, current_node, node_manager->get_scope_id(current_node));
            }
            default:
                continue;
        }

        // TODO: type checking and resolution
    }
}

bool SageCompiler::check_filename_valid(string filename) {
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

    set<int> available_registers;
    map<string, int> current_reg_assignments;
    set<string> working_symbols;
    for (int r = GENERAL_REG_RANGE_BEGIN; r < GENERAL_REG_RANGE_END; ++r) {
        available_registers.insert(r);
    }

    int scope_range_max = scope_manager.scopes.size()-1;
    for (int current_scope = 0; current_scope < scope_range_max; ++current_scope) {
        auto new_symbols = scope_manager.in_scope_identifiers(node_manager, current_scope);
        working_symbols.insert(new_symbols.begin(), new_symbols.end());

        for (auto symbol_ident: working_symbols) {
            auto symbol_address = symbol_table.lookup_from_scope(symbol_ident, current_scope);
            if (symbol_address == NULL_INDEX) {
                // expire old symbol
                available_registers.insert(current_reg_assignments[symbol_ident]);
                current_reg_assignments.erase(symbol_ident);
                continue;
            }

            // if symbol is not already allocated
            if (current_reg_assignments.find(symbol_ident) == current_reg_assignments.end()) {
                auto next_available_register = *available_registers.begin();
                available_registers.erase(next_available_register);

                current_reg_assignments[symbol_ident] = next_available_register;
                symbol_table.lookup_by_index(symbol_address)->assigned_register = next_available_register;
            }
        }
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

bool SageCompiler::volatile_is_stale(SageValue& live, int volatile_idx) {
    return !volatile_register_state[volatile_idx].equals(live);
}
































