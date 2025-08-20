#include <stdlib.h>
#include <stack>
/*#include <llvm/IR/Module.h>*/
#include <boost/algorithm/string.hpp>
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/Support/TargetSelect.h"
// #include "llvm/Target/TargetMachine.h"
// #include "llvm/Target/TargetOptions.h"
// #include "llvm/MC/TargetRegistry.h"
// #include "llvm/Support/FileSystem.h"
// #include "llvm/Support/Host.h"
// #include "llvm/Support/raw_ostream.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/Path.h"
// #include "llvm/IR/LegacyPassManager.h"
// #include "llvm/IR/Verifier.h"

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"
#include "../include/depgraph.h"

using namespace std;
/*using namespace llvm;*/

SageCompiler::SageCompiler() {}

SageCompiler::SageCompiler(string mainfile)
    : ast(NULL_INDEX),
      debug(COMPILATION),
      node_manager(new NodeManager()),
      parser(SageParser(node_manager, mainfile)),
      interpreter(new SageInterpreter(4046)),
      symbol_table(SageSymbolTable()) {

    symbol_table.initialize();

    current_procedure.push(0);
    procedures.push_back(vector<command>());
}

SageCompiler::~SageCompiler() {
    delete node_manager;
    delete interpreter;
}

NodeIndex SageCompiler::parse_codefile(string target_file) {
    parser.filename = target_file;
    NodeIndex parsetree = parser.parse_program(false);
    if (parsetree == NULL_INDEX) {
        printf("parsetree root is null. parsing failed.\n");
        return NULL_INDEX;
    }
    return parsetree;
}

bool SageCompiler::compile(NodeIndex ast) {
    if (ast == NULL_INDEX || node_manager->get_host_nodetype(ast) != PN_BLOCK) {
        return false;
    }

    // TODO: need to figure out eventually how to work with packages and have the linker link previously compiled sage object files to the current project
    for (auto child : node_manager->get_children(ast)) {
        if (node_manager->get_nodetype(child) == PN_INCLUDE) {
            string codefile = node_manager->get_lexeme(child);
            codefile.erase(std::remove(codefile.begin(), codefile.end(), '"'), codefile.end());
            codefile = codefile + ".sage";
            codefile = "sage_modules/" + codefile;
            NodeIndex file_ast = parse_codefile(codefile);
            this->visitor.visit_program(file_ast);
        }
    }

    this->visitor.visit_program(ast);

    size_t total_size = 0;
    for (const auto& vec : visitor.procedures) {
        total_size += vec.size();
    }
    visitor.total_bytecode.reserve(total_size);

    for (const auto& vec : visitor.procedures) {
        visitor.total_bytecode.insert(visitor.total_bytecode.end(), vec.begin(), vec.end());
    }

    return true;
}

// successful SageCompiler::emit_and_link_llvm(llvm::Module* module, const std::string& output_file) {
//     llvm::InitializeAllTargetInfos();
//     llvm::InitializeAllTargets();
//     llvm::InitializeAllTargetMCs();
//     llvm::InitializeAllAsmParsers();
//     llvm::InitializeAllAsmPrinters();
// 
//     // Get target machine
//     auto target_triple = "x86_64-pc-linux-gnu";
//     module->setTargetTriple(target_triple);
// 
//     std::string error;
//     const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
// 
//     if (target == nullptr) {
//         printf("Target not found.\n");
//         return false;
//     }
// 
//     auto CPU = "x86-64";
//     auto features = "";
//     llvm::TargetOptions options;
//     auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Static);
//     auto target_machine = target->createTargetMachine(target_triple, CPU, features, options, RM);
// 
//     // Set up module layout and triple
//     module->setDataLayout(target_machine->createDataLayout());
//     module->setTargetTriple(target_triple);
// 
//     // Create object file
//     std::error_code EC;
//     llvm::raw_fd_ostream dest(output_file, EC, llvm::sys::fs::OF_None);
//     if (EC) {
//         printf("Could not open file.\n");
//         return false;
//     }
// 
//     llvm::legacy::PassManager pass;
//     auto file_type = llvm::CGFT_ObjectFile;
//     if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
//         llvm::errs() << "TargetMachine can't emit a file of this type\n";
//         return false;
//     }
// 
//     if (module == nullptr) {
//         printf("MODULE IS NULL\n");
//     }
// 
//     if (debug == COMPILATION || debug == ALL) {
//         // for debug purposes
//         module->print(llvm::outs(), nullptr);
//     }
// 
//     std::string verify_error;
//     if (llvm::verifyModule(*module, &llvm::errs())) {
//         printf("Module verification failed\n");
//         return false;
//     }
// 
//     pass.run(*module);
//     dest.flush();
// 
//     std::string link_cmd = "clang -no-pie sage.out -o sage.run";
//     system(link_cmd.c_str());
// 
//     return true;
// }

void SageCompiler::begin_compilation(string mainfile) {
    if (!check_filename_valid(mainfile)) {
        printf("Main program filename is not valid, Make sure it ends in '.sage'.\n");
        return;
    }

    NodeIndex program_parsetree = parser.parse_program(debug == PARSING || debug == ALL);
    if (program_parsetree == -1) {
        printf("Program parsetree root was null. Parsing failed.\n");
        return;
    }

    if (debug >= COMPILATION) {
        node_manager->showtree(program_parsetree);
    }

    // 1. program static analysis
    auto dep_graph = generate_ident_dependencies(root, "global", 0);
    if (!dep_graph->dependencies_are_valid()) {
        // report compiler errors
        return;
    }

    register_allocation(dep_graph, root);

    // TODO: Type checking

    // 2. program compilation
    // TODO: Run run directives
    for (auto start_node : dep_graph->get_comptime_exec_order()) {
        bool success = compile(start_node);
        if (!success) {
            printf("Compilation failed.\n");
            return;
        }
        interpreter->load_program(runtime_)
    }

    bool success = compile(program_parsetree);
    if (!success) {
        // report compiler errors
        printf("Compilation failed.\n");
        return;
    }

    ///////////////
    interpreter->load_program(runtime_bytecode);
    interpreter->execute();
    interpreter->close();
    // ^^^^^^^^TEMP!!!

    // 3. Compiling to target instructions (if sagevm not the target)

    // 4. Linking
    /*bool success = emit_and_link_llvm(module, "sage.out"); */

    if (success) {
        printf("Compilation finished successfully.\n");
    }else {
        printf("Compilation finished unsuccessfully.\n");
    }
}

bool SageCompiler::check_filename_valid(string filename) {
    // validate that file is valid
    if (filename.find('.') == string::npos) {
        printf("cannot target files that have no file extension.\n");
        return false;
    }

    vector<string> delimited;
    boost::split(delimited, filename, boost::is_any_of("."));

    if (delimited[1] != "sage") {
        printf("cannot target non sage source files.\n");
        return false;
    }

    return true;
}

DependencyGraph* SageCompiler::generate_ident_dependencies(NodeIndex cursor, string scopename, int scopelevel, set<string>* parent_scope) {
    if (cursor != NULL_INDEX && manager->get_host_nodetype(cursor) != PN_BLOCK) {
        return nullptr;
    }

    DependencyGraph* dependencies = new DependencyGraph(scopename, scopelevel, parent_scope);
    ParseNodeType reptype;
    DependencyGraph* nested_dependency;

    // auto parse_sub_dependencies = [this, dependencies, nested_dependency](NodeIndex cursor, string name, int scope) mutable {
    //     nested_dependency = generate_ident_dependencies(cursor, name, scope, &dependencies->local_scope);
    //     dependencies->merge_with(*nested_dependency);
    // };

    auto get_expression_identifiers = [&, this] (NodeIndex root) -> vector<string> {
        vector<string> identifiers;
        switch (manager->get_host_nodetype(root)) {
            case PN_UNARY:
                identifiers.push_back(manager->get_lexeme(root));
                break;
            case PN_BINARY: {
                auto left_idents = get_expression_identifiers(manager->get_left(root));
                auto right_idents = get_expression_identifiers(manager->get_right(root));
                identifiers.insert(identifiers.begin(), left_idents.begin(), left_idents.end());
                identifiers.insert(identifiers.begin(), right_idents.begin(), right_idents.end());
                break;
            }
            case PN_TRINARY: {
                auto left_idents = get_expression_identifiers(manager->get_left(root));
                auto middle_idents = get_expression_identifiers(manager->get_middle(root));
                auto right_idents = get_expression_identifiers(manager->get_right(root));
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

    dependencies->local_scope = set<string>();

    for (auto child : manager->get_children(cursor)) {
        reptype = manager->get_nodetype(cursor);
        switch (reptype) {
            case PN_FUNCDEF:
            case PN_STRUCT:
                nested_dependency = generate_ident_dependencies(manager->get_right(cursor), manager->get_lexeme(cursor), scopelevel++, &dependencies->local_scope);
                dependencies->add_scope_node(manager->get_lexeme(cursor), INI, cursor, nested_dependency);
                continue;
            case PN_WHILE:
                // parse_sub_dependencies(manager->get_right(cursor), "", scopelevel++);
                nested_dependency = generate_ident_dependencies(manager->get_right(cursor), "", scopelevel++, &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_FOR:
                // parse_sub_dependencies(manager->get_right(cursor), "", scopelevel++);
                nested_dependency = generate_ident_dependencies(manager->get_right(cursor), "", scopelevel++, &dependencies->local_scope);
                dependencies->merge_with(*nested_dependency);
                continue;
            case PN_RUN_DIRECTIVE:
                // parse_sub_dependencies(manager->get_branch(cursor), "", scopelevel--);
                nested_dependency = generate_ident_dependencies(manager->get_branch(cursor), "", scopelevel--, &dependencies->local_scope);
                dependencies->add_scope_node("", INI, cursor, nested_dependency);
                continue;
            case PN_IF: {
                for (auto child : manager->get_children(cursor)) {
                    nested_dependency = generate_ident_dependencies(
                        child,
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
                string function_name = manager->get_lexeme(cursor);
                for (auto child_index : manager->get_children(manager->get_right(cursor))) {
                    dependencies->add_connection(function_name, manager->get_lexeme(child_index));
                }

                continue;
            }
            case PN_ASSIGN: {
                // assigned depends on its assignees
                string assigned_var_name = manager->get_lexeme(cursor);

                // rhs is a tree of unary, binary and trinary nodes representing operations and operands (or just a single value)
                auto referenced_identifiers = get_expression_identifiers(manager->get_right(cursor));

                for (string ident : referenced_identifiers) {
                    dependencies->add_connection(assigned_var_name, ident);
                }
                
                continue;
            }

            case PN_VAR_DEC:
                dependencies->add_node(manager->get_lexeme(cursor), INI, cursor);
                continue;
            // NOTE: case PN_VAR_REF: won't occurr on the statement level but will be found on the expression level, look for these in the three above
            default:
                printf("DEPENDENCY RESOLUTION SCAN MISSING: %s\n", nodetype_to_string(reptype).c_str());
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

    auto allocate_registers = [this, &](DependencyGraph* graph, vector<u64>* prioritized_vars) {
        for (auto var : *prioritize_vars) {
            if (available_general_regs.size() == 0) {
                symbol_table.declare_symbol(graph->nodes[var].name, -1); // -1 register_alloc indicates spilled
                continue;
            }
            auto chosen = available_general_regs.begin();
            available_general_regs.erase(chosen);
            symbol_table.declare_symbol(graph->nodes[var].name, chosen); 
        }
    };

    set<string> currently_allocated;

    auto expire_old_interval = [this, &](DependencyGraph* walk) {
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
