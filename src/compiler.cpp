#include <stack>
#include <queue>
#include <numeric>
#include <algorithm>
#include <thread>
#include <cassert>
#include <filesystem>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <array>

#include "codegen.h"
#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"
#include "../include/scope_manager.h"

using namespace std;

SageCompiler::SageCompiler(CompilerOptions options)
    : options(options),
      node_manager(new NodeManager()),
      scope_manager(ScopeManager()),
      parser(SageParser(&scope_manager, node_manager)),
      builder(BytecodeBuilder()),
      comptime_manager(ComptimeManager(node_manager)),
      dependency_graph(ScopeDependencyGraph(this)) {
    // Set scope_manager on node_manager for automatic scope_id assignment
    node_manager->set_scope_manager(&scope_manager);
}

SageCompiler::~SageCompiler() {
    delete node_manager;
}

bool SageCompiler::generating_compile_time_bytecode() {
    return codegen_mode == GEN_COMPTIME;
}

void SageCompiler::compile_file(string mainfile) {
    assertm(options.compilation_target == SAGE_VM, sen("Support for compilation target", compilation_target_string(options.compilation_target), "is not implemented yet.").data());

    if (options.debug == LEXING) {
        parser.print_lexer_output(mainfile);
        return;
    }

    /// 1. INITIAL PROGRAM COMPILATION PASS
    string emit_string;
    NodeIndex ast_root = parser.parse_program(mainfile);
    assertm(ast_root != NULL_INDEX, "AST root is null. parsing failed.");
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    if (options.debug >= PARSING) node_manager->showtree(ast_root);

    int symbol_count = ceil((BUILTIN_COUNT + parser.symbol_count) * 1.5);
    symbol_table = SageSymbolTable(&scope_manager, node_manager, symbol_count);
    symbol_table.initialize();

    scan_all_program_symbols(ast_root);

    comptime_manager.register_task_dependencies(symbol_table);
    bool comptime_is_valid = comptime_manager.verify_comptime_dependencies();
    if (!comptime_is_valid) {
        logger.report_errors();
        return;
    }
    comptime_manager.static_program_memory = &static_program_memory_store;

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

    // 2. COMPTIME EXECUTION AND PROCESSING
    if (!comptime_manager.tasks.empty()) {
        comptime_manager.set_symbol_table(&symbol_table);
        codegen_mode = GEN_COMPTIME;
        bool on_last_batch = false;

        assert(comptime_manager.task_min_heap.top()->prerequisite_tasks.size() == 0);

        // get every task in current execution batch
        int current_prerequisite_count = 0;
        vector<ComptimeTask *> task_execution_batch;
        while (!on_last_batch) {
            assertm(comptime_manager.execution_iterations < comptime_manager.MAX_ITERATIONS || !comptime_manager.task_min_heap.empty(), "Reached maximum compile time iteration count. This should never happen");

            while (current_prerequisite_count == comptime_manager.get_next_task_prerequisite_count()) {
                task_execution_batch.push_back(comptime_manager.task_min_heap.top());
                comptime_manager.task_min_heap.pop();

                if (comptime_manager.task_min_heap.empty()) {
                    on_last_batch = true;
                    break;
                }
            }
            current_prerequisite_count = comptime_manager.get_next_task_prerequisite_count();

            // if no tasks in current batch then stop
            // for each task generate its bytecode
            for (auto *task: task_execution_batch) {
                builder.reset_and_exit_comptime();
                builder.enter_comptime();
                visit(task->associated_ast_root);
                map<int, int> procedure_line_locations;
                task->task_instructions = builder.finalize_comptime_bytecode(procedure_line_locations);
                if (options.emit_bytecode) emit_string += builder.emit();
                // builder.print_bytecode(task->task_instructions);
                task->procedure_to_instruction_index = procedure_line_locations;
                comptime_manager.staged_for_execution.push(task);
            }
            if (logger.has_errors()) {
                logger.report_errors();
                return;
            }

            // execute each task in parallel
            int thread_count = std::min((int) comptime_manager.tasks.size(), (int) std::thread::hardware_concurrency());
            comptime_manager.execute_tasks_in_parallel(thread_count);
            if (logger.has_errors()) {
                logger.report_errors();
                return;
            }

            // propogate comptime constants to symbol table
            for (auto *finished_task: task_execution_batch) {
                // skip any tasks that aren't associated with a symbol pending task execution
                if (symbol_table.comptime_task_id_to_symbol_id.find(finished_task->task_id) == symbol_table.
                    comptime_task_id_to_symbol_id.end()) {
                    continue;
                }
                SymbolIndex symbol_index = symbol_table.comptime_task_id_to_symbol_id[finished_task->task_id];
                symbol_table.entries.get_pointer(symbol_index)->data = finished_task->symbol_injection_value;
            }

            if (comptime_manager.modifies_runtime_ast()) {
                // TODO
                // 1. perform AST modifications
                // 2. rescan modified AST sections for symbols and do type resolution and
                //    forward decl resolution for modified AST subtrees
            }

            // perform anymore needed context updating
        }
        builder.reset_and_exit_comptime();
    }

    /// 3. SAGE RUNTIME BYTECODE GENERATION
    codegen_mode = GEN_RUNTIME;
    visit(ast_root);
    if (logger.has_errors()) {
        logger.report_errors();
        return;
    }

    map<int, int> procedure_to_instruction_index;
    bytecode runtime_code = builder.finalize_runtime_bytecode(procedure_to_instruction_index);
    if (options.debug_print_bytecode) builder.print_bytecode(runtime_code);

    if (options.emit_bytecode) {
        emit_string += "\n\n" + builder.emit();
        std::filesystem::path filepath = ".sage/bytecode/s.asm";
        std::filesystem::create_directories(filepath.parent_path());

        FILE *file = fopen(".sage/bytecode/s.asm", "w");
        if (file) {
            fwrite(emit_string.data(), 1, emit_string.size(), file);
            fclose(file);
        }
    }

    if (options.compilation_target == SAGE_VM) {
        auto interpreter = SageInterpreter(&symbol_table);
        interpreter.open(procedure_to_instruction_index, static_program_memory_store);
        interpreter.load_program(runtime_code);
        interpreter.execute();
        interpreter.close();

        if (logger.has_errors()) {
            logger.report_errors();
        }
        return;
    }

    // switch (options.compilation_target) {
    //     case X86:
    //         // TODO: target_llvm_x86(runtime_code, options.output_file);
    //         break;
    //     case X86_64:
    //         // TODO: target_llvm_x86_64(runtime_code, options.output_file);
    //         break;
    //     case ARM32:
    //         // TODO: target_llvm_arm32(runtime_code, options.output_file);
    //         break;
    //     case ARM64:
    //         // TODO: target_llvm_arm64(runtime_code, options.output_file);
    //         break;
    //     case AARCH64_64:
    //         // TODO: target_llvm_aarch64_64(runtime_code, options.output_file);
    //         break;
    //     case RISCV:
    //         // TODO: target_llvm_riscv(runtime_code, options.output_file);
    //         break;
    //     case WEBASM:
    //         // TODO: target_llvm_webasm(runtime_code, options.output_file);
    //         break;
    //     default:
    //         break;
    // }

    if (logger.has_errors()) {
        logger.report_errors();
    }
}

void SageCompiler::scan_all_program_symbols(NodeIndex current_node, int function_parameter_register, string parent_function_name) {
    auto nodetype = node_manager->get_nodetype(current_node);
    switch (nodetype) {
        case PN_STRUCT: {
            symbol_table.declare_type_symbol(current_node, nullptr);

            auto body_node = node_manager->get_branch(node_manager->get_right(current_node));
            for (auto child: node_manager->get_children(body_node)) {
                scan_all_program_symbols(child);
            }

            break;
        }
        case PN_FUNCDEF: {
            auto table_index = symbol_table.declare_function(current_node, nullptr);
            string function_identifier = node_manager->get_identifier(current_node);

            symbol_table.entries.get_pointer(table_index)->max_return_count = parser.
                function_to_max_return_count[function_identifier];

            auto signature_trinary_node = node_manager->get_right(current_node);
            auto paramters_node = node_manager->get_left(signature_trinary_node);
            auto bodynode = node_manager->reach_right(current_node, 2);

            int current_parameter_register = 0;
            for (auto child: node_manager->get_children(paramters_node)) {
                scan_all_program_symbols(child, current_parameter_register, function_identifier);
                current_parameter_register++;
            }

            // scan the return type node
            scan_all_program_symbols(node_manager->get_middle(signature_trinary_node));

            for (auto child: node_manager->get_children(bodynode)) {
                scan_all_program_symbols(child);
            }

            break;
        }
        case PN_VAR_DEC: {
            auto identifier = node_manager->get_identifier(current_node);
            SymbolIndex new_variable_symbol = parent_function_name != "" ?
                symbol_table.declare_parameter(current_node, nullptr, function_parameter_register) :
                symbol_table.declare_variable(current_node, nullptr);

            auto right_most_node = node_manager->get_right(current_node);
            if (node_manager->get_host_nodetype(current_node) == PN_BINARY) {
                if (node_manager->get_nodetype(right_most_node) == PN_TYPE) {
                    string type_identifier = node_manager->get_identifier(right_most_node);
                    symbol_table.entries.get_pointer(new_variable_symbol)->spilled = type_identifier.find("*") != std::string::npos;
                }

            }else if (node_manager->get_host_nodetype(current_node) == PN_TRINARY) {
                auto declaration_type_node = node_manager->get_middle(current_node);
                if (declaration_type_node != NULL_INDEX && node_manager->get_nodetype(declaration_type_node) == PN_TYPE) {
                    string type_identifier = node_manager->get_identifier(declaration_type_node);
                    symbol_table.entries.get_pointer(new_variable_symbol)->spilled = type_identifier.find("*") != std::string::npos;
                }

                auto assigned_value_node = node_manager->get_right(current_node);
                if (node_manager->get_nodetype(assigned_value_node) == PN_RUN_DIRECTIVE) {
                    symbol_table.register_comptime_value(comptime_manager, current_node, new_variable_symbol);

                    auto bodynode = node_manager->get_branch(current_node);
                    for (auto child: node_manager->get_children(bodynode)) {
                        scan_all_program_symbols(child);
                    }

                    break;
                }
            }

            scan_all_program_symbols(right_most_node);
            break;
        }
        case PN_FOR:
        case PN_IF:
        case PN_IF_BRANCH:
        case PN_ELSE_BRANCH:
        case PN_WHILE: {
            auto bodynode = node_manager->get_right(current_node);
            for (auto child: node_manager->get_children(bodynode)) {
                scan_all_program_symbols(child);
            }

            break;
        }
        case PN_RUN_DIRECTIVE: {
            comptime_manager.add_task(current_node);
            auto bodynode = node_manager->get_branch(current_node);
            for (auto child: node_manager->get_children(bodynode)) {
                scan_all_program_symbols(child);
            }

            break;
        }
        case PN_FUNCCALL: {
            auto identifier = node_manager->get_identifier(current_node);

            scan_all_program_symbols(node_manager->get_branch(current_node));
            break;
        }
        case PN_KEYWORD: {
            if (node_manager->get_branch(current_node) == NULL_INDEX) return;
            scan_all_program_symbols(node_manager->get_branch(current_node));
            break;
        }
        case PN_STRING: {
            auto node_lexeme = node_manager->get_lexeme(current_node);
            node_lexeme.erase(std::remove(node_lexeme.begin(), node_lexeme.end(), '"'), node_lexeme.end());
            auto *string_symbol = symbol_table.lookup(
                node_manager->get_identifier(current_node),
                node_manager->get_scope_id(current_node)
            );
            if (string_symbol != nullptr) return;

            process_escape_sequences(node_lexeme);
            int64_t string_length = node_lexeme.size();

            vector<SageType *> string_member_types = {
                TR::get_pointer_type(TR::get_byte_type(CHAR)), TR::get_integer_type(8)
            };
            auto *string_type = TR::get_struct_type("string", string_member_types);

            int64_t static_pointer = static_program_memory_store.size();
            static_program_memory_store.resize(static_program_memory_store.size() + string_length);
            std::memcpy(&static_program_memory_store[static_pointer], node_lexeme.c_str(), string_length);

            ByteVector string_instance_data;
            string_instance_data.resize(string_type->size);
            std::memcpy(string_instance_data.data(), &static_pointer, 8);
            std::memcpy(&string_instance_data[8], &string_length, 8);

            SageValue string_value = SageValue(string_type, string_instance_data);
            symbol_table.declare_literal(current_node, string_value, static_pointer);
            break;
        }
        default: {
            switch (node_manager->get_host_nodetype(current_node)) {
                case PN_BLOCK: {
                    for (auto child: node_manager->get_children(current_node)) {
                        scan_all_program_symbols(child);
                    }
                    break;
                }
                case PN_TRINARY:
                case PN_BINARY: {
                    scan_all_program_symbols(node_manager->get_right(current_node));
                    break;
                }
                case PN_UNARY: {
                    auto branch = node_manager->get_branch(current_node);
                    if (branch == NULL_INDEX) return;

                    if (nodetype != PN_POINTER_DEREFERENCE && nodetype != PN_POINTER_REFERENCE) {
                        scan_all_program_symbols(branch);
                        return;
                    }

                    if (node_manager->get_nodetype(branch) != PN_VAR_REF) {
                        scan_all_program_symbols(branch);
                        return;
                    }

                    auto identifier = node_manager->get_identifier(current_node);

                    auto variable_reference_identifier = node_manager->get_identifier(branch);
                    auto scope_id = node_manager->get_scope_id(branch);
                    auto search_entry = symbol_table.lookup(variable_reference_identifier, scope_id);
                    if (search_entry != nullptr) {
                        search_entry->spilled = true;
                    }else {
                        symbol_table.identifiers_that_must_be_spilled.insert(variable_reference_identifier);
                    }
                }
                default:
                    break;
            }
        }
    }
}

void SageCompiler::perform_type_resolution() {
    vector<SymbolIndex> program_symbols(symbol_table.entries.size);
    std::iota(program_symbols.begin(), program_symbols.end(), 0);

    for (SymbolIndex index: program_symbols) {
        auto *entry = symbol_table.entries.get_pointer(index);
        if (symbol_table.builtins.find(index) != symbol_table.builtins.end()) continue;
        if (entry->type_is_resolved()) continue;

        auto nodetype = node_manager->get_nodetype(entry->definition_ast_index);
        switch (nodetype) {
            case PN_VAR_DEC:
                entry->datatype = symbol_table.resolve_variable_type(index);
                break;
            case PN_STRUCT:
                entry->datatype = symbol_table.resolve_struct_type(index);
                break;
            case PN_FUNCDEF:
                entry->datatype = symbol_table.resolve_function_type(index);
                break;
            default:
                assertm(false, "Type resolution encountered unknown symbol type");
                break;
        }
    }
}

string compilation_target_string(CompilationTarget target) {
    switch (target) {
        case SAGE_VM:
            return "SageVM";
        case X86:
            return "X86";
        case X86_64:
            return "X86_64";
        case ARM32:
            return "ARM32";
        case ARM64:
            return "ARM64";
        case AARCH64_64:
            return "AArch64";
        case RISCV:
            return "RISC";
        case WEBASM:
            return "Web Assembly";
        default:
            return "Unknown target.";
    }
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

    vector<SymbolIndex> variables_by_scope(symbol_table.variables.begin(), symbol_table.variables.end());
    //variables_by_scope.insert(variables_by_scope.end(), symbol_table.constants.begin(), symbol_table.constants.end());

    // then arange what is left according to scope_id descending
    std::sort(variables_by_scope.begin(), variables_by_scope.end(), [this](int a, int b) {
        return symbol_table.entries.get(a).scope_id < symbol_table.entries.get(b).scope_id;
    });

    set<int> available_registers;
    set<SymbolIndex> working_symbols;
    int current_scope = 0;
    int current_relative_stack_location = 0;
    for (int r = GENERAL_REG_RANGE_BEGIN; r < GENERAL_REG_RANGE_END; ++r) {
        available_registers.insert(r);
    }

    int assigned_register;
    for (SymbolIndex index: variables_by_scope) {
        auto &entry = symbol_table.entries.get(index);

        if (entry.is_struct_member) {
            continue;
        }

        if (current_scope != entry.scope_id) {
            current_relative_stack_location = 0;
            current_scope = entry.scope_id;

            // Free registers for symbols whose scopes are no longer active (not ancestors of current scope)
            for (auto it = working_symbols.begin(); it != working_symbols.end();) {
                auto &working_entry = symbol_table.entries.get(*it);
                if (scope_manager.is_ancestor_of(working_entry.scope_id, current_scope)) {
                    // Symbol's scope is an ancestor of current scope -> still visible, keep register
                    ++it;
                    continue;
                }
                // Symbol's scope is NOT an ancestor -> we've exited that scope, free register
                available_registers.insert(working_entry.assigned_register);
                it = working_symbols.erase(it);
            }
        }

        if (entry.spilled || entry.datatype->size > 8 || entry.datatype->is_pointer() || entry.datatype->is_struct()) {
            symbol_table.entries.get_pointer(index)->spill(current_relative_stack_location);
            current_relative_stack_location += entry.datatype->size;
            continue;
        }

        if (available_registers.empty()) {
            // no room -> spill
            symbol_table.entries.get_pointer(index)->spill(current_relative_stack_location);
            current_relative_stack_location += entry.datatype->size;
            continue;
        }

        assigned_register = *available_registers.begin();
        symbol_table.entries.get_pointer(index)->assigned_register = assigned_register;
        working_symbols.insert(index);
        available_registers.erase(assigned_register);
    }
}

int SageCompiler::get_volatile_register() {
    int _register = 10 + (volatile_index % VOLATILE_REGISTER_SIZE);
    volatile_index++;
    return _register;
}

void SageCompiler::ScopeDependencyGraph::add_definition_contents_to_dependency_graph(NodeIndex current_node) {
    auto *node_manager = compiler->node_manager;
    auto &symbol_table = compiler->symbol_table;
    auto &logger = compiler->logger;

    switch (compiler->node_manager->get_host_nodetype(current_node)) {
        case PN_UNARY: {
            auto nodetype = node_manager->get_nodetype(current_node);
            if (nodetype != PN_IDENTIFIER && nodetype != PN_VAR_REF && nodetype != PN_TYPE && nodetype != PN_FUNCCALL) {
                auto branch_index = node_manager->get_branch(current_node);
                if (branch_index == -1) return;
                add_definition_contents_to_dependency_graph(branch_index);
                break;
            }

            auto identifier = node_manager->get_identifier(current_node);
            auto symbol = symbol_table.lookup(identifier, local_scope);
            if (symbol == nullptr) return;
            if (previously_processed.find(identifier) != previously_processed.end()) return;
            if (symbol_table.builtins.find(symbol->symbol_index) != symbol_table.builtins.end()) return;

            if (symbol->definition_ast_index == -1) {
                Token found_tok = node_manager->get_token(current_node);
                logger.log_error_unsafe(found_tok, sen("Undefined reference:", identifier), SEMANTIC);
                return;
            }

            // if the found reference is in scope of working scope
            // then add data dependency and increment in degree
            int identifier_index = local_defintions_to_matrix_index[identifier];
            int root_definition_identifier_index = local_defintions_to_matrix_index[root_definition_identifier];
            if (pair_seen_previously(root_definition_identifier_index, identifier_index)) return;

            mark(root_definition_identifier_index, identifier_index);
            break;
        }
        case PN_BINARY: {
            auto left = node_manager->get_left(current_node);
            auto right = node_manager->get_right(current_node);
            add_definition_contents_to_dependency_graph(left);
            add_definition_contents_to_dependency_graph(right);
            break;
        }
        case PN_TRINARY: {
            auto left = node_manager->get_left(current_node);
            auto middle = node_manager->get_middle(current_node);
            auto right = node_manager->get_right(current_node);
            add_definition_contents_to_dependency_graph(left);
            add_definition_contents_to_dependency_graph(middle);
            add_definition_contents_to_dependency_graph(right);
            break;
        }
        case PN_BLOCK: {
            for (auto child: node_manager->get_children(current_node)) {
                add_definition_contents_to_dependency_graph(child);
            }
            break;
        }
        default:
            break;
    }
}

void SageCompiler::ScopeDependencyGraph::initialize_graph(
    set<string> local_definition_identifiers,
    int local_scope
) {

    auto &symbol_table = compiler->symbol_table;
    this->local_scope = local_scope;

    local_defintions_to_matrix_index.clear();
    matrix_index_to_definition_identifier.clear();

    int i = 0;
    for (auto identifier: local_definition_identifiers) {
        auto *column_symbol = symbol_table.global_lookup(identifier);
        if (column_symbol == nullptr) continue; // the function which feeds into this parameter includes identifiers that aren't truly definition symbols
        if (symbol_table.builtins.find(column_symbol->symbol_index) != symbol_table.builtins.end()) continue;

        local_defintions_to_matrix_index[identifier] = i;
        matrix_index_to_definition_identifier[i] = identifier;
        i++;
    }

    col_row_length = local_defintions_to_matrix_index.size();
    int matrix_size = col_row_length*col_row_length;
    local_scope_definition_dependency_matrix = new int[matrix_size];
    for (i = 0; i < matrix_size; ++i) {
        local_scope_definition_dependency_matrix[i] = 0;
    }
}

void SageCompiler::ScopeDependencyGraph::setup_definition_fringe(
    queue<string> &fringe,
    map<string, NodeIndex> &identifier_to_ast,
    map<string, int> &in_degrees
) {
    auto &symbol_table = compiler->symbol_table;
    int matrix_size = col_row_length*col_row_length;

    vector<int> column_sums;
    column_sums.resize(col_row_length);

    for (int i = 0; i < matrix_size; ++i) {
        int column_index = i % col_row_length;
        column_sums[column_index] += local_scope_definition_dependency_matrix[i];
    }

    for (int column = 0; column < col_row_length; ++column) {
        string column_identifier = matrix_index_to_definition_identifier[column];
        in_degrees[column_identifier] = column_sums[column];
        if (column_sums[column] != 0) continue;

        auto ast_id = symbol_table.global_lookup(column_identifier)->definition_ast_index;
        identifier_to_ast[column_identifier] = ast_id;
        fringe.push(column_identifier);
    }
}

void SageCompiler::ScopeDependencyGraph::resolve_definition_order() {
    auto *node_manager = compiler->node_manager;
    auto &scope_manager = compiler->scope_manager;
    auto &logger = compiler->logger;

    vector<NodeIndex> result_order;
    queue<string> fringe;
    map<string, NodeIndex> identifier_to_ast;
    map<string, int> in_degrees;
    setup_definition_fringe(fringe, identifier_to_ast, in_degrees);

    set<string> visited;
    while (!fringe.empty()) {
        string current_identifier = fringe.front();
        fringe.pop();
        int current_index = local_defintions_to_matrix_index[current_identifier];

        if (visited.find(current_identifier) != visited.end()) {
            auto token = node_manager->get_token(identifier_to_ast[current_identifier]);
            logger.log_error_unsafe(token, sen("Invalid redefinition of symbol:", current_identifier), SEMANTIC);
            break;
        }
        visited.insert(current_identifier);
        NodeIndex current_ast_index = identifier_to_ast[current_identifier];
        // if current is trinary var dec then we need to push back a new binary dec node
        if (node_manager->get_nodetype(current_ast_index == PN_VAR_DEC) &&
            node_manager->get_host_nodetype(current_ast_index) == PN_TRINARY) {
            auto left_node = node_manager->get_left(current_ast_index);
            auto middle_node = node_manager->get_middle(current_ast_index);
            Token token = node_manager->get_token(current_ast_index);
            auto new_dec_node = node_manager->create_binary(token, PN_VAR_DEC, left_node, middle_node);
            result_order.push_back(new_dec_node);
        }else {
            result_order.push_back(current_ast_index);
        }

        int row_index = current_index; // index of defintion that comes before
        for (int i = row_index * col_row_length; i < (row_index + 1) * col_row_length; ++i) {
            if (local_scope_definition_dependency_matrix[i] == 0) continue;
            int column_index = i % col_row_length;
            string column_identifier = matrix_index_to_definition_identifier[column_index];
            in_degrees[column_identifier] -= 1;
            if (in_degrees[column_identifier] == 0) {
                auto ast_id = compiler->symbol_table.global_lookup(column_identifier)->definition_ast_index;
                identifier_to_ast[column_identifier] = ast_id;
                fringe.push(column_identifier);
            }
        }
    }

    if (result_order.empty()) { return; }

    NodeIndex target_ast_root = scope_manager.scope_to_astroot[local_scope];

    // preserve order of non definition statements in scope while prepending new resolved defintion statement order
    for (auto child: node_manager->get_children(target_ast_root)) {
        auto nodetype = node_manager->get_nodetype(child);
        // if current child is a trinary var dec then we need to push back a new binary assign node using nodes from the trinary dec
        if (nodetype == PN_VAR_DEC && node_manager->get_host_nodetype(child) == PN_TRINARY) {
            auto left_node = node_manager->get_left(child);
            auto right_node = node_manager->get_right(child);
            Token token = node_manager->get_token(child);
            auto new_dec_node = node_manager->create_binary(token, PN_ASSIGN, left_node, right_node);
            result_order.push_back(new_dec_node);
            continue;
        }

        if (nodetype == PN_FUNCDEF || nodetype == PN_VAR_DEC || nodetype == PN_STRUCT) {
            continue;
        }

        result_order.push_back(child);
    }

    node_manager->set_children(target_ast_root, result_order);
}

void SageCompiler::forward_declaration_resolution(int program_root) {
    vector<SymbolIndex> definitions_by_scope(symbol_table.variables.begin(), symbol_table.variables.end());
    definitions_by_scope.insert(definitions_by_scope.end(), symbol_table.types.begin(), symbol_table.types.end());
    definitions_by_scope.insert(definitions_by_scope.end(), symbol_table.functions.begin(),
                                symbol_table.functions.end());
    std::sort(definitions_by_scope.begin(), definitions_by_scope.end(), [this](int a, int b) {
        return (symbol_table.entries.get(a).scope_id < symbol_table.entries.get(b).scope_id);
    });
    definitions_by_scope.push_back(SAGE_NULL_SYMBOL);

    set<string> local_definition_identifiers;
    auto get_local_identifiers = [&](int local_scope) {
        local_definition_identifiers.clear();
        for (int i = 0; i < symbol_table.entries.size; ++i) {
            auto &entry = symbol_table.entries.data[i];
            bool not_builtin = symbol_table.builtins.find(entry.symbol_index) == symbol_table.builtins.end();
            if (entry.scope_id == local_scope && not_builtin) {
                local_definition_identifiers.insert(entry.name);
            }
        }
    };

    int current_scope = node_manager->get_scope_id(program_root);
    get_local_identifiers(current_scope);
    dependency_graph.initialize_graph(local_definition_identifiers, current_scope);

    // for each scope, find every native definition and get its in_degree,
    // then sort those definitions into a valid compilation order
    //  *** in_degree represents amount of in scope references contained within the definition
    for (int symbol_id: definitions_by_scope) {
        if (symbol_id == SAGE_NULL_SYMBOL || current_scope != symbol_table.entries.get(symbol_id).scope_id) {
            dependency_graph.resolve_definition_order();
            dependency_graph.delete_graph();
            if (symbol_id == SAGE_NULL_SYMBOL) break;

            current_scope = symbol_table.entries.get(symbol_id).scope_id;
            get_local_identifiers(current_scope);
            dependency_graph.initialize_graph(local_definition_identifiers, current_scope);
        }

        auto ast_id = symbol_table.entries.get(symbol_id).definition_ast_index;
        if (ast_id == -1) continue;

        auto nodetype = node_manager->get_nodetype(ast_id);
        assert(nodetype == PN_FUNCDEF || nodetype == PN_VAR_DEC || nodetype == PN_STRUCT);

        NodeIndex definition_contents = nodetype == PN_STRUCT ?
            node_manager->get_branch(node_manager->get_right(ast_id)) :
            node_manager->get_right(ast_id);

        dependency_graph.root_definition_identifier = node_manager->get_identifier(ast_id);
        dependency_graph.add_definition_contents_to_dependency_graph(definition_contents);
    }
}
