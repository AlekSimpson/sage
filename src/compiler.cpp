#include <stack>
#include <queue>
#include <numeric>
#include <algorithm>
#include <thread>
#include <cassert>
#include <filesystem>
#include <cstring>
#include <cstdint>
#include <array>

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
      comptime_manager(ComptimeManager(node_manager)){
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
    if (options.compilation_target != SAGE_VM) {
        logger.log_internal_error_unsafe(
            "compiler.cpp",
            current_linenum,
            sen("Support for compilation target", compilation_target_string(options.compilation_target), "is not implemented yet."));
    }

    /// 1. INITIAL PROGRAM COMPILATION PASS
    string emit_string;
    NodeIndex ast_root = parser.parse_program(mainfile);
    if (ast_root == NULL_INDEX) {
        logger.log_internal_error_unsafe("compiler.cpp", current_linenum, "AST root is null. parsing failed.");
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

    comptime_manager.register_task_dependencies(symbol_table);
    bool comptime_is_valid = comptime_manager.verify_comptime_dependencies();
    if (!comptime_is_valid) {
        logger.report_errors();
        return;
    }
    comptime_manager.static_program_memory = static_program_memory;

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
            if (comptime_manager.execution_iterations >= comptime_manager.MAX_ITERATIONS &&
                comptime_manager.task_min_heap.empty()) {
                logger.log_internal_error_unsafe(
                    "compiler.cpp",
                    current_linenum,
                    "Reached maximum compile time iteration count. This should never happen");
                break;
            }

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
            int thread_count = std::min((int)comptime_manager.tasks.size(), (int)std::thread::hardware_concurrency());
            comptime_manager.execute_tasks_in_parallel(thread_count);
            if (logger.has_errors()) {
                logger.report_errors();
                return;
            }

            // propogate comptime constants to symbol table
            for (auto *finished_task: task_execution_batch) {
                // skip any tasks that aren't associated with a symbol pending task execution
                if (symbol_table.comptime_task_id_to_symbol_id.find(finished_task->task_id) == symbol_table.comptime_task_id_to_symbol_id.end()) {
                    continue;
                }
                table_index symbol_index = symbol_table.comptime_task_id_to_symbol_id[finished_task->task_id];
                symbol_table.entries.get_pointer(symbol_index)->value = finished_task->symbol_injection_value;
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
    builder.print_bytecode(runtime_code);

    if (options.emit_bytecode) {
        emit_string += "\n\n" + builder.emit();
        std::filesystem::path filepath = ".sage/bytecode/s.asm";
        std::filesystem::create_directories(filepath.parent_path());

        FILE* file = fopen(".sage/bytecode/s.asm", "w");
        if (file) {
            fwrite(emit_string.data(), 1, emit_string.size(), file);
            fclose(file);
        }
    }

    if (options.compilation_target == SAGE_VM) {
        auto interpreter = SageInterpreter(&symbol_table);
        interpreter.open(procedure_to_instruction_index, static_program_memory);
        interpreter.load_program(runtime_code);
        interpreter.execute();
        interpreter.close();

        if (logger.has_errors()) {
            logger.report_errors();
        }
        printf("compilation successful.\n");
        return;
    }

    switch (options.compilation_target) {
        case X86:
            // TODO: target_llvm_x86(runtime_code, options.output_file);
            break;
        case X86_64:
            // TODO: target_llvm_x86_64(runtime_code, options.output_file);
            break;
        case ARM32:
            // TODO: target_llvm_arm32(runtime_code, options.output_file);
            break;
        case ARM64:
            // TODO: target_llvm_arm64(runtime_code, options.output_file);
            break;
        case AARCH64_64:
            // TODO: target_llvm_aarch64_64(runtime_code, options.output_file);
            break;
        case RISCV:
            // TODO: target_llvm_riscv(runtime_code, options.output_file);
            break;
        case WEBASM:
            // TODO: target_llvm_webasm(runtime_code, options.output_file);
            break;
        default:
            break;
    }

    if (logger.has_errors()) {
        logger.report_errors();
    }
    printf("compilation successful.\n");
}

std::array<uint8_t, sizeof(double)> double_to_bytes(double value) {
    std::array<uint8_t, sizeof(double)> bytes;
    std::memcpy(bytes.data(), &value, sizeof(double));
    return bytes;
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
                auto table_idx = symbol_table.declare_function(current_node, nullptr);
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
                table_index new_variable_symbol;
                if (parameter_name_to_parent_function.find(identifier) != parameter_name_to_parent_function.end()) {
                    string parent_function_name = parameter_name_to_parent_function[identifier];
                    new_variable_symbol = symbol_table.declare_parameter(current_node, nullptr, function_parameter_register_generator[parent_function_name]);
                    function_parameter_register_generator[parent_function_name]++;
                }else {
                    new_variable_symbol = symbol_table.declare_variable(current_node, nullptr);
                }

                // if the right most child ast node is a run directive that means that this variable is going to be
                // awaiting the compile time computed output of its child run directive
                if (node_manager->get_nodetype(node_manager->get_right(current_node)) == PN_RUN_DIRECTIVE) {
                    symbol_table.register_comptime_value(comptime_manager, current_node, new_variable_symbol);

                    auto bodynode = node_manager->get_branch(current_node);
                    for (auto child: node_manager->get_children(bodynode)) {
                        fringe.push(child);
                    }
                    continue;
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
                comptime_manager.add_task(current_node);
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
                auto *array_type = TypeRegistery::get_array_type(char_type, node_lexeme.length());
                auto index = symbol_table.declare_literal(current_node, SageValue());
                symbol_table.entries.get_pointer(index)->type = array_type;

                static_program_memory_insertion_order.push_back(index);
                static_program_memory[index] = vector<uint8_t>();
                static_program_memory[index].reserve(node_lexeme.size());
                for (int i = 0; i < (int)node_lexeme.size(); ++i) {
                    static_program_memory[index].push_back(static_cast<uint8_t>(node_lexeme[i]));
                }

                continue;
            }
            case PN_FLOAT: {
                auto *float_type = TypeRegistery::get_builtin_type(FLOAT, 8);
                auto index = symbol_table.declare_literal(current_node, SageValue());
                //symbol_table.declare_literal(current_node, SageValue());
                symbol_table.entries.get_pointer(index)->type = float_type;

                string node_lexeme = node_manager->get_lexeme(current_node);
                double raw_float_value = stof(node_lexeme);
                auto bytes = double_to_bytes(raw_float_value);

                static_program_memory_insertion_order.push_back(index);
                static_program_memory[index] = vector<uint8_t>();
                static_program_memory[index].reserve(8);
                static_program_memory[index].insert(static_program_memory[index].begin(), bytes.begin(), bytes.end());

                continue;
            }
            case PN_NUMBER: {
                //auto node_lexeme = node_manager->get_lexeme(current_node);
                //SageValue integer_literal = SageValue(stoi(node_lexeme));
                //symbol_table.declare_immediate(integer_literal, node_lexeme);
                continue;
            }
            default:
                continue;
        }
    }

    return;
}

void SageCompiler::perform_type_resolution() {
    vector<table_index> program_symbols(symbol_table.entries.size);
    std::iota(program_symbols.begin(), program_symbols.end(), 0);

    for (table_index idx: program_symbols) {
        auto *entry = symbol_table.entries.get_pointer(idx);
        if (symbol_table.builtins.find(idx) != symbol_table.builtins.end()) continue;
        if (entry->type_is_resolved()) continue;

        auto nodetype = node_manager->get_nodetype(entry->definition_ast_index);
        switch (nodetype) {
            case PN_VAR_DEC:
                entry->type = symbol_table.resolve_variable_type(idx);
                break;
            case PN_STRUCT:
                entry->type = symbol_table.resolve_struct_type(idx);
                break;
            case PN_FUNCDEF:
                entry->type = symbol_table.resolve_function_type(idx);
                break;
            default:
                logger.log_internal_error_unsafe(
                    "compiler.cpp",
                    current_linenum,
                    "Type resolution encountered unknown symbol type");
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

    vector<table_index> variables_by_scope(symbol_table.variables.begin(), symbol_table.variables.end());
    variables_by_scope.insert(variables_by_scope.end(), symbol_table.constants.begin(), symbol_table.constants.end());

    // then arange what is left according to scope_id descending
    std::sort(variables_by_scope.begin(), variables_by_scope.end(), [this](int a, int b) {
        return symbol_table.entries.get(a).scope_id < symbol_table.entries.get(b).scope_id;
    });

    set<int> available_registers;
    set<string> working_symbols;
    int current_scope = 0;
    int current_relative_stack_location = 0;
    for (int r = GENERAL_REG_RANGE_BEGIN; r < GENERAL_REG_RANGE_END; ++r) {
        available_registers.insert(r);
    }

    symbol_entry *working_symbol_entry;
    int assigned_register;
    for (table_index idx: variables_by_scope) {
        auto entry = symbol_table.entries.get(idx);
        if (current_scope != entry.scope_id) {
            current_relative_stack_location = 0;
            current_scope = entry.scope_id;
        }

        if (entry.type->identify() == FLOAT) {
            symbol_table.entries.get(idx).spill(current_relative_stack_location);
            current_relative_stack_location += entry.type->size;
            continue;
        }

        for (auto symbol: working_symbols) {
            working_symbol_entry = symbol_table.lookup(symbol, current_scope);
            if (working_symbol_entry != nullptr) continue;

            working_symbols.erase(symbol);
            available_registers.insert(working_symbol_entry->assigned_register);
        }

        if (available_registers.empty()) {
            // no room -> spill
            symbol_table.entries.get(idx).spill(current_relative_stack_location);
            current_relative_stack_location += entry.type->size;
            continue;
        }

        assigned_register = *available_registers.begin();
        symbol_table.entries.get_pointer(idx)->assigned_register = assigned_register;
        working_symbols.insert(entry.identifier);
        available_registers.erase(assigned_register);
    }
}

int SageCompiler::get_volatile_register() {
    int _register = 10 + (volatile_index % VOLATILE_REGISTER_SIZE);
    volatile_index++;
    return _register;
}

int SageCompiler::get_volatile_float_register() {
    int _register = 70 + (volatile_float_index % VOLATILE_FLOAT_REGISTER_SIZE);
    volatile_float_index++;
    return _register;
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
                logger.log_error_unsafe(found_tok, sen("Undefined reference:", identifier), SEMANTIC);
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
            logger.log_error_unsafe(token, sen("Invalid redefinition of symbol:", current), SEMANTIC);
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
        return (symbol_table.entries.get(a).scope_id < symbol_table.entries.get(b).scope_id);
    });
    definitions_by_scope.push_back(SAGE_NULL_SYMBOL);

    int current_scope = node_manager->get_scope_id(program_root);
    in_degree_map.clear();
    previously_processed.clear();

    // for each scope, find every native definition and get its in_degree,
    // then sort those definitions into a valid compilation order
    //  *** in_degree represents amount of in sope references contained within the definition
    for (int symbol_id: definitions_by_scope) {
        if (current_scope != symbol_table.entries.get(symbol_id).scope_id) {
            resolve_definition_order(current_scope);
            if (symbol_id == SAGE_NULL_SYMBOL) break;

            current_scope = symbol_table.entries.get(symbol_id).scope_id;

            for (const auto &[identifier, in_degree]: in_degree_map) {
                previously_processed.insert(identifier);
            }
            in_degree_map.clear();
            definition_dependencies.clear();
        }

        auto ast_id = symbol_table.entries.get(symbol_id).definition_ast_index;
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






