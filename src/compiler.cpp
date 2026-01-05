#include <stdlib.h>
#include <stack>
#include <boost/algorithm/string.hpp>
#include <sys/syscall.h>

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"
#include "../include/depgraph.h"
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

    interpreter = new SageInterpreter(4046);

    // setup builtin functions
    vector<SageType*> puti_params = {
        TypeRegistery::get_builtin_type(I64),
        TypeRegistery::get_builtin_type(I64)
    };
    symbol_table.declare_symbol("puti", TypeRegistery::get_function_type(puti_params, vector<SageType*>()));

    vector<SageType*> puts_params = {
        TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(CHAR)),
        TypeRegistery::get_builtin_type(I64)
    };
    symbol_table.declare_symbol("puts", TypeRegistery::get_function_type(puts_params, vector<SageType*>()));

    bookmarked_run_directives = ascending_list<comptime_ast_bookmark>(node_manager->get_node_count());

    // TODO: wrap debug options and output into error logger
    // node_manager->showtree(ast_root);

    // 1. program static analysis

    // TODO: Type checking

    set<string> parent_scope;
    builder.builtins = {"puts", "puti"};
    auto dep_graph = generate_ident_dependencies(ast_root, "global", 0, &parent_scope);
    if (dep_graph == nullptr) {
        logger.report_errors();
        return;
    }

    !dep_graph->dependencies_are_valid(); // do a sweep for any identifier resolution errors

    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    register_allocation(dep_graph);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // 2. program compilation

    // execute run directives
    interpreter_mode = true;
    comptime_ast_bookmark mark;
    string rundir_name;
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

    // todo: test that functions declared inside functions work
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    delete dep_graph;

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

void SageCompiler::get_expression_identifiers(vector<NodeIndex>& identifiers, NodeIndex root) {
    switch (node_manager->get_host_nodetype(root)) {
        case PN_UNARY: {
            auto nodetype = node_manager->get_nodetype(root);
            if (nodetype == PN_IDENTIFIER || nodetype == PN_VAR_REF) {
                identifiers.push_back(root);
                return;
            }
            vector<NodeIndex> branch_idents;
            if (node_manager->get_branch(root) != -1) {
                get_expression_identifiers(branch_idents, node_manager->get_branch(root));
                identifiers.insert(identifiers.begin(), branch_idents.begin(), branch_idents.end());
            }

            return;
        }
        case PN_BINARY: {
            vector<NodeIndex> left_idents;
            vector<NodeIndex> right_idents;
            get_expression_identifiers(left_idents, node_manager->get_left(root));
            get_expression_identifiers(right_idents, node_manager->get_right(root));
            identifiers.insert(identifiers.begin(), left_idents.begin(), left_idents.end());
            identifiers.insert(identifiers.begin(), right_idents.begin(), right_idents.end());
            return;
        }
        case PN_TRINARY: {
            vector<NodeIndex> left_idents;
            vector<NodeIndex> middle_idents;
            vector<NodeIndex> right_idents;
            get_expression_identifiers(left_idents, node_manager->get_left(root));
            get_expression_identifiers(middle_idents, node_manager->get_middle(root));
            get_expression_identifiers(right_idents, node_manager->get_right(root));
            identifiers.insert(identifiers.begin(), left_idents.begin(), left_idents.end());
            identifiers.insert(identifiers.begin(), middle_idents.begin(), middle_idents.end());
            identifiers.insert(identifiers.begin(), right_idents.begin(), right_idents.end());
        }
        case PN_BLOCK: {
            for (auto child : node_manager->get_children(root)) {
                vector<NodeIndex> idents;
                idents.reserve(node_manager->get_children(root).size());
                get_expression_identifiers(idents, child);
                identifiers.insert(identifiers.begin(), idents.begin(), idents.end());
            }
        }
        default:
            break;
    }
};

DependencyGraph* SageCompiler::generate_ident_dependencies(
    NodeIndex cursor,
    string scopename,
    int scopelevel,
    set<string>* parent_scope
) {
    if (cursor == NULL_INDEX) {
        logger.log_internal_error("compiler.cpp", current_linenum, "generate_ident_dependencies recieved null cursor");
        return nullptr;
    }
    if (node_manager->get_host_nodetype(cursor) != PN_BLOCK) {
        string message = str("generate_ident_dependencies: param cursor expected to be BLOCK type, instead was", nodetype_to_string(node_manager->get_host_nodetype(cursor)));
        logger.log_internal_error("compiler.cpp", current_linenum, message);
        return nullptr;
    }

    DependencyGraph* dependencies = new DependencyGraph(node_manager, scopename, scopelevel, parent_scope);
    ParseNodeType reptype;
    DependencyGraph* nested_dependency;

    dependencies->local_scope = set<string>();

    // setup builtins
    for (auto builtin : builder.builtins) {
        dependencies->add_node(builtin, INI, -1);
        dependencies->local_scope.insert(builtin);
    }

    for (auto child : node_manager->get_children(cursor)) {
        reptype = node_manager->get_nodetype(child);
        switch (reptype) {
            case PN_FUNCDEF: {
                auto name = node_manager->get_identifier(child);
                nested_dependency = generate_ident_dependencies(
                    node_manager->reach_right(child, 2),
                    name,
                    scopelevel++,
                    &dependencies->local_scope);

                // process function parameters
                vector<NodeIndex> idents;
                get_expression_identifiers(idents, node_manager->get_left(node_manager->get_right(child)));
                for (int i = 0; i < (int)idents.size(); ++i) {
                    string param_name = node_manager->get_lexeme(idents[i]);
                    nested_dependency->add_param_node(param_name, INI, idents[i]);
                    auto entry_idx = symbol_table.declare_symbol(param_name, i);
                    symbol_table.entries[entry_idx].is_parameter = true;
                    
                    // Early-bind parameter
                    node_manager->set_resolved_symbol(idents[i], entry_idx);
                }

                dependencies->add_scope_node(name, INI, child, nested_dependency);
                int func_symbol_idx = symbol_table.declare_symbol(name, -1);
                
                // Early-bind the function name identifier
                NodeIndex func_name_node = node_manager->get_left(child);
                if (func_name_node != NULL_INDEX) {
                    node_manager->set_resolved_symbol(func_name_node, func_symbol_idx);
                }
                continue;
            }
            case PN_STRUCT: {
                auto name = node_manager->get_identifier(child);
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    node_manager->get_identifier(child),
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->add_scope_node(node_manager->get_identifier(child), INI, child, nested_dependency);
                symbol_table.declare_symbol(name, -1);
                continue;
            }
            case PN_WHILE:
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    "",
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_FOR:
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    "",
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_RUN_DIRECTIVE:
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_branch(child),
                    "",
                    scopelevel--,
                    &dependencies->local_scope);
                dependencies->add_scope_node(
                    str("#run", node_manager->get_token(child).linenum),
                    INI,
                    child,
                    nested_dependency);
                bookmarked_run_directives.insert(
                    comptime_ast_bookmark{nested_dependency, child, scopelevel},
                    ast_bookmark_sorter
                );
                continue;
            case PN_IF: {
                for (auto grandchild : node_manager->get_children(child)) {
                    nested_dependency = generate_ident_dependencies(
                        grandchild,
                        "",
                        scopelevel++,
                        &dependencies->local_scope
                    );
                    if (nested_dependency->nodes.size() == 0) {
                        continue;
                    }

                    dependencies->merge_with(*nested_dependency);
                }
                continue;
            }
            case PN_FUNCCALL: {
                // caller depends on its parameters
                string function_name = node_manager->get_identifier(child);
                dependencies->add_node(function_name, REF, child);
                
                // Early-bind function reference if possible
                int func_scope = node_manager->get_scope_id(child);
                int func_symbol_idx = symbol_table.lookup_from_scope(function_name, func_scope);
                if (func_symbol_idx != -1) {
                    node_manager->set_resolved_symbol(child, func_symbol_idx);
                }
                
                vector<NodeIndex> idents;
                get_expression_identifiers(idents, node_manager->get_branch(child));
                for (auto ident : idents) {
                    dependencies->add_connection(function_name, node_manager->get_lexeme(ident), ident);
                    
                    // Early-bind argument identifiers
                    string ident_name = node_manager->get_lexeme(ident);
                    int ident_scope = node_manager->get_scope_id(ident);
                    int ident_symbol_idx = symbol_table.lookup_from_scope(ident_name, ident_scope);
                    if (ident_symbol_idx != -1) {
                        node_manager->set_resolved_symbol(ident, ident_symbol_idx);
                    }
                }

                continue;
            }
            case PN_ASSIGN: {
                // assigned depends on its assignees
                string assigned_var_name = node_manager->get_identifier(child);
                int symbol_idx;
                if (node_manager->get_host_nodetype(child) == PN_BINARY) {
                    dependencies->add_node(assigned_var_name, REF, child);
                    // Early-bind the assigned variable reference
                    int child_scope = node_manager->get_scope_id(child);
                    symbol_idx = symbol_table.lookup_from_scope(assigned_var_name, child_scope);
                }else {
                    dependencies->add_node(assigned_var_name, INI, child);
                    symbol_idx = symbol_table.declare_symbol(assigned_var_name, -1);
                }
                
                // Early-bind the LHS identifier
                NodeIndex lhs = node_manager->get_left(child);
                if (lhs != NULL_INDEX && symbol_idx != -1) {
                    node_manager->set_resolved_symbol(lhs, symbol_idx);
                }

                // rhs is a tree of unary, binary and trinary nodes representing operations and operands (or just a single value)
                vector<NodeIndex> referenced_identifiers;
                get_expression_identifiers(referenced_identifiers, node_manager->get_right(child));
                for (NodeIndex ident : referenced_identifiers) {
                    dependencies->add_connection(assigned_var_name, node_manager->get_lexeme(ident), ident);
                    
                    // Early-bind RHS identifiers
                    string ident_name = node_manager->get_lexeme(ident);
                    int ident_scope = node_manager->get_scope_id(ident);
                    int ident_symbol_idx = symbol_table.lookup_from_scope(ident_name, ident_scope);
                    if (ident_symbol_idx != -1) {
                        node_manager->set_resolved_symbol(ident, ident_symbol_idx);
                    }
                }
                
                continue;
            }

            case PN_VAR_DEC: {
                string var_name = node_manager->get_identifier(child);
                dependencies->add_node(var_name, INI, child);
                int symbol_idx = symbol_table.declare_symbol(var_name, -1);
                
                // Early-bind the declared variable
                NodeIndex name_node = node_manager->get_left(child);
                if (name_node != NULL_INDEX) {
                    node_manager->set_resolved_symbol(name_node, symbol_idx);
                }
                continue;
            }

            case PN_VAR_REF: {
                string var_name = node_manager->get_identifier(child);
                dependencies->add_node(var_name, REF, child);
                
                // Early-bind the variable reference
                int child_scope = node_manager->get_scope_id(child);
                int symbol_idx = symbol_table.lookup_from_scope(var_name, child_scope);
                if (symbol_idx != -1) {
                    node_manager->set_resolved_symbol(child, symbol_idx);
                }
                continue;
            }

            case PN_KEYWORD: {
                if (node_manager->get_lexeme(child) != "ret") {
                    continue;
                }

                vector<NodeIndex> idents;
                get_expression_identifiers(idents, node_manager->get_branch(child));
                for (auto ident : idents) {
                    string ident_name = node_manager->get_identifier(ident);
                    dependencies->add_node(ident_name, REF, child);
                    
                    // Early-bind return expression identifiers
                    int ident_scope = node_manager->get_scope_id(ident);
                    int symbol_idx = symbol_table.lookup_from_scope(ident_name, ident_scope);
                    if (symbol_idx != -1) {
                        node_manager->set_resolved_symbol(ident, symbol_idx);
                    }
                }

                continue;
            }

            default:
                logger.log_internal_error("compiler.cpp", current_linenum, "dependency resolution scan missing");
                break;
        }
    }

    return dependencies;
}

void SageCompiler::allocate_registers(
    DependencyGraph* graph,
    vector<int>* prioritized_vars,
    set<int>* available_general_regs,
    stack<int>* virtual_stack_frame,
    set<string>* currently_allocated) {
    for (auto var : *prioritized_vars) {
        if (graph->nodes[var].is_parameter) continue;

        auto var_symbol = symbol_table.lookup(graph->nodes[var].name);
        if (var_symbol->type != nullptr && var_symbol->type->identify() == FUNC) continue;

        if (available_general_regs->size() == 0) {
            var_symbol->assigned_register = -1;
            var_symbol->spilled = true;
            var_symbol->spill_offset = virtual_stack_frame->top();
            virtual_stack_frame->top()++;
            continue;
        }
        int chosen = *available_general_regs->begin();
        available_general_regs->erase(chosen);
        var_symbol->assigned_register = chosen;
        currently_allocated->insert(graph->nodes[var].name);
    }
}

void SageCompiler::expire_old_intervals(
    DependencyGraph* walk,
    set<string>* currently_allocated,
    set<int>* available_general_regs
    ) {
    set<string> fullscope;
    fullscope.insert(walk->parent_scope->begin(), walk->parent_scope->end());
    fullscope.insert(walk->local_scope.begin(), walk->local_scope.end());

    set<string> expired_values;
    set_difference(currently_allocated->begin(), currently_allocated->end(),
                   fullscope.begin(), fullscope.end(),
                   inserter(expired_values, expired_values.begin()));
    for (string value : expired_values) {
        int value_register = symbol_table.lookup(value)->assigned_register;
        available_general_regs->insert(value_register);
        currently_allocated->erase(value);
    }

    set_difference(fullscope.begin(), fullscope.end(),
                   expired_values.begin(), expired_values.end(),
                   inserter(*currently_allocated, currently_allocated->begin()));
}

void SageCompiler::update_scope(
    DependencyGraph* walk,
    set<string>* currently_allocated,
    stack<int>* virtual_stack_frame
    ) {
    auto push_vframe = [virtual_stack_frame] {
        virtual_stack_frame->push(0);
    };

    auto pop_vframe = [virtual_stack_frame]() {
        virtual_stack_frame->pop();
    };

    set<string> fullscope;
    fullscope.insert(walk->parent_scope->begin(), walk->parent_scope->end());
    fullscope.insert(walk->local_scope.begin(), walk->local_scope.end());

    set<string> expired_values;
    set_difference(currently_allocated->begin(), currently_allocated->end(),
                   fullscope.begin(), fullscope.end(),
                   inserter(expired_values, expired_values.begin()));

    // if we have expired values then something went out of scope -> pop
    if (expired_values.size() > 0) {
        pop_vframe();
        return;
    }

    set<string> result;
    // if there are things in fullscope that are not currently allocated -> push
    set_difference(fullscope.begin(), fullscope.end(),
                   currently_allocated->begin(), currently_allocated->end(),
                   inserter(result, result.begin()));
    if (result.size() > 0) {
        push_vframe();
    }
}

void SageCompiler::register_allocation(DependencyGraph* dependencies) {
    /*
     *  Factors to consider for variable-register mapping sorting order
     *  1. Usage amount throughout scope
     *  2. Local to scope means higher priority
     *  3. Is it used in a loop?
     *
     *  need to track current scope to expire old intervals, the scopes lifetime essentially is the variable lifetime for all vars
     *
     *  while loop: stack not empty
     *   1. pop stack -> walk var
     *   2. expire old intervals
     *   3. get values declared in current scope
     *   4. sort them and allocate them, allocations written to symbol table
     *   5. add next branches to stack
     */

    stack<int> virtual_stack_frame;
    set<int> available_general_regs;
    for (int r = GENERAL_REG_RANGE_BEGIN; r < GENERAL_REG_RANGE_END; ++r) {
        available_general_regs.insert(r);
    }

    set<string> currently_allocated;

    stack<DependencyGraph*> fringe;
    DependencyGraph* walk;
    fringe.push(dependencies);
    while (!fringe.empty()) {
        walk = fringe.top();
        fringe.pop();
        update_scope(walk, &currently_allocated, &virtual_stack_frame);

        vector<int> buffer;
        buffer.reserve(walk->nodes.size());

        if (available_general_regs.size() != 100) {
            expire_old_intervals(walk, &currently_allocated, &available_general_regs);
        }

        for (auto [key, node] : walk->nodes) {
            if (node.owned_scope != nullptr) {
                fringe.push(node.owned_scope);
                continue;
            }
            if (node.is_parameter) continue;

            buffer.push_back(key);
        }
        if (!buffer.empty()) {
            walk->quicksort(&buffer);
            allocate_registers(walk, &buffer, &available_general_regs, &virtual_stack_frame, &currently_allocated);
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
































