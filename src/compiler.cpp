#include <stdlib.h>
#include <stack>
#include <boost/algorithm/string.hpp>

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"
#include "../include/depgraph.h"

using namespace std;

int ast_bookmark_sorter(comptime_ast_bookmark mark) {
    return mark.scope_level;
}

SageCompiler::SageCompiler() {}

SageCompiler::SageCompiler(string mainfile)
    : ast(NULL_INDEX),
      debug(COMPILATION),
      node_manager(new NodeManager()),
      parser(SageParser(node_manager, mainfile)),
      interpreter(new SageInterpreter(4046)),
      symbol_table(SageSymbolTable()),
      builder(BytecodeBuilder()) {
    symbol_table.initialize();
}

SageCompiler::~SageCompiler() {
    delete node_manager;
    delete interpreter;
}

NodeIndex SageCompiler::parse_codefile(string target_file) {
    parser.filename = target_file;
    NodeIndex parsetree = parser.parse_program(false);
    if (parsetree == NULL_INDEX) {
        logger.log_internal_error("compiler.cpp", 40, "AST root is null. parsing failed.");
        return NULL_INDEX;
    }
    return parsetree;
}

bytecode SageCompiler::compile(NodeIndex ast_index, DependencyGraph* dep_graph) {
     if (ast == NULL_INDEX || node_manager->get_host_nodetype(ast) != PN_BLOCK) {
        return bytecode();
    }

    visit_codeblock(ast_index, dep_graph);
    if (logger.has_errors()) {
        logger.report_errors();
    }

    bytecode output = builder.final();
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
        if (logger.has_errors()) {
            logger.report_errors();
            return;
        }

        logger.log_internal_error("compiler.cpp", 75, "AST root was null. Parsing failed.");
        return;
    }

    if (debug >= COMPILATION) {
        node_manager->showtree(ast_root);
    }

    // 1. program static analysis
    set<string> parent_scope;
    auto dep_graph = generate_ident_dependencies(ast_root, "global", 0, &parent_scope);
    if (!dep_graph->dependencies_are_valid()) {
        logger.report_errors();
        return;
    }

    register_allocation(dep_graph);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    // TODO: Type checking

    // 2. program compilation
    comptime_ast_bookmark mark;
    for (int i = 0; i < bookmarked_run_directives.size; ++i) {
        mark = bookmarked_run_directives[i];
        bytecode code = compile(mark.ast_position, mark.graph);
        precompiled.insert(mark.ast_position);
        interpreter->load_program(code);
        interpreter->execute();
    }

    // runtime code compilation
    bytecode code = compile(ast_root, dep_graph);

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

vector<string> SageCompiler::get_expression_identifiers(NodeIndex root) {
    vector<string> identifiers;
    switch (node_manager->get_host_nodetype(root)) {
        case PN_UNARY:
            identifiers.push_back(node_manager->get_lexeme(root));
            break;
        case PN_BINARY: {
            auto left_idents = get_expression_identifiers(node_manager->get_left(root));
            auto right_idents = get_expression_identifiers(node_manager->get_right(root));
            identifiers.insert(identifiers.begin(), left_idents.begin(), left_idents.end());
            identifiers.insert(identifiers.begin(), right_idents.begin(), right_idents.end());
            break;
        }
        case PN_TRINARY: {
            auto left_idents = get_expression_identifiers(node_manager->get_left(root));
            auto middle_idents = get_expression_identifiers(node_manager->get_middle(root));
            auto right_idents = get_expression_identifiers(node_manager->get_right(root));
            identifiers.insert(identifiers.begin(), left_idents.begin(), left_idents.end());
            identifiers.insert(identifiers.begin(), middle_idents.begin(), middle_idents.end());
            identifiers.insert(identifiers.begin(), right_idents.begin(), right_idents.end());
            break;
        }
        default:
            break;
    }

    return identifiers;
};

DependencyGraph* SageCompiler::generate_ident_dependencies(
    NodeIndex cursor,
    string scopename,
    int scopelevel,
    set<string>* parent_scope
) {
    if (cursor != NULL_INDEX && node_manager->get_host_nodetype(cursor) != PN_BLOCK) {
        logger.log_internal_error("compiler.cpp", 160, "generate_ident_dependencies recieved null cursor or cursor was a BLOCK type.");
        return nullptr;
    }

    DependencyGraph* dependencies = new DependencyGraph(node_manager, scopename, scopelevel, parent_scope);
    ParseNodeType reptype;
    DependencyGraph* nested_dependency;

    // auto parse_sub_dependencies = [this, dependencies, nested_dependency](NodeIndex cursor, string name, int scope) mutable {
    //     nested_dependency = generate_ident_dependencies(cursor, name, scope, &dependencies->local_scope);
    //     dependencies->merge_with(*nested_dependency);
    // };

    dependencies->local_scope = set<string>();

    for (auto child : node_manager->get_children(cursor)) {
        reptype = node_manager->get_nodetype(child);
        switch (reptype) {
            case PN_FUNCDEF:
            case PN_STRUCT:
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    node_manager->get_lexeme(child),
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->add_scope_node(node_manager->get_lexeme(child), INI, child, nested_dependency);
                continue;
            case PN_WHILE:
                // parse_sub_dependencies(manager->get_right(cursor), "", scopelevel++);
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    "",
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_FOR:
                // parse_sub_dependencies(manager->get_right(cursor), "", scopelevel++);
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_right(child),
                    "",
                    scopelevel++,
                    &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_RUN_DIRECTIVE:
                // parse_sub_dependencies(manager->get_branch(cursor), "", scopelevel--);
                nested_dependency = generate_ident_dependencies(
                    node_manager->get_branch(child),
                    "",
                    scopelevel--,
                    &dependencies->local_scope);
                dependencies->add_scope_node("", INI, child, nested_dependency);
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
                string function_name = node_manager->get_lexeme(child);
                for (auto child_index : node_manager->get_children(node_manager->get_right(child))) {
                    dependencies->add_connection(function_name, node_manager->get_lexeme(child_index));
                }

                continue;
            }
            case PN_ASSIGN: {
                // assigned depends on its assignees
                string assigned_var_name = node_manager->get_lexeme(child);

                // rhs is a tree of unary, binary and trinary nodes representing operations and operands (or just a single value)
                auto referenced_identifiers = get_expression_identifiers(node_manager->get_right(child));

                for (string ident : referenced_identifiers) {
                    dependencies->add_connection(assigned_var_name, ident);
                }
                
                continue;
            }

            case PN_VAR_DEC:
                dependencies->add_node(node_manager->get_lexeme(child), INI, child);
                continue;
            // NOTE: case PN_VAR_REF: won't occurr on the statement level but will be found on the expression level, look for these in the three above
            default:
                logger.log_internal_error("compiler.cpp", 292, "dependency resolution scan missing");
                break;
        }
    }

    return dependencies;
}

void SageCompiler::register_allocation(DependencyGraph* dependencies) {
    // Factors to consider for variable-register mapping sorting order
    // 1. Usage amount throughout scope
    // 2. Local to scope means higher priority
    // 3. Is it used in a loop?

    // need to track current scope to expire old intervals, the scopes lifetime essentially is the variable lifetime for all vars

    // while loop: stack not empty
    //  1. pop stack -> walk var
    //  2. expire old intervals
    //  3. get values declared in current scope
    //  4. sort them and allocate them, allocations written to symbol table
    //  5. add next branches to stack

    set<int> available_general_regs;
    for (int r = GENERAL_REG_RANGE_BEGIN; r <= GENERAL_REG_RANGE_END; ++r) {
        available_general_regs.insert(r);
    }

    auto allocate_registers = [&, this](DependencyGraph* graph, vector<u64>* prioritized_vars) {
        for (auto var : *prioritized_vars) {
            if (available_general_regs.size() == 0) {
                symbol_table.declare_symbol(graph->nodes[var].name, -1); // -1 register_alloc indicates spilled
                continue;
            }
            int chosen = *available_general_regs.begin();
            available_general_regs.erase(chosen);
            symbol_table.declare_symbol(graph->nodes[var].name, chosen); 
        }
    };

    set<string> currently_allocated;

    auto expire_old_intervals = [&, this](DependencyGraph* walk) {
        set<string> fullscope;
        fullscope.insert(walk->parent_scope->begin(), walk->parent_scope->end());
        fullscope.insert(walk->local_scope.begin(), walk->local_scope.end());

        set<string> expired_values;
        set_difference(currently_allocated.begin(), currently_allocated.end(),
                       fullscope.begin(), fullscope.end(),
                       inserter(expired_values, expired_values.begin()));
        for (string value : expired_values) {
            int value_register = symbol_table.lookup(value)->assigned_register;
            available_general_regs.insert(value_register);
        }

        set_difference(fullscope.begin(), fullscope.end(),
                       expired_values.begin(), expired_values.end(),
                       inserter(currently_allocated, currently_allocated.begin()));
    };

    stack<DependencyGraph*> fringe;
    DependencyGraph* walk;
    fringe.push(dependencies);
    while (!fringe.empty()) {
        walk = fringe.top();
        fringe.pop();

        vector<u64> buffer;
        buffer.reserve(walk->nodes.size());

        expire_old_intervals(walk);

        for (auto [key, node] : walk->nodes) {
            if (node.owned_scope != nullptr) {
                fringe.push(node.owned_scope);
                continue;
            }

            buffer.push_back(key);
        }
        walk->quicksort(&buffer);
        allocate_registers(walk, &buffer);
    }
}

bool SageCompiler::node_is_precompiled(NodeIndex node) {
    return precompiled.find(node) != precompiled.end();
}
