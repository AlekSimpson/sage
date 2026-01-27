#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <stack>

#include "../include/error_logger.h"
#include "../include/comptime_manager.h"
#include "../include/interpreter.h"
#include "../include/symbols.h"

using namespace std;

int ComptimeManager::add_task(NodeIndex ast_index) {
    if (node_to_task_id.find(ast_index) != node_to_task_id.end()) {
        return node_to_task_id[ast_index];
    }
    // TODO: we need to check if this is a #inject directive, cause if so then we need to set the result_type accordingly
    //       and we need to lock: "modifies_runtime_ast_lock"

    int scope_id = node_man->get_scope_id(ast_index);
    ComptimeTask new_task = ComptimeTask(tasks.size(), scope_id, ast_index, TaskResultType::VALUE);
    tasks.push_back(new_task);
    task_min_heap.push(&tasks.back());
    node_to_task_id[ast_index] = tasks.size()-1;
    return tasks.size()-1;
}

void ComptimeManager::register_task_dependencies(SageSymbolTable &symbol_table) {
    set<table_index> current_comptime_prerequisites;
    NodeIndex current_ast_index;
    string current_identifier;
    int current_scope_id;
    ParseNodeType nodetype;
    NodeIndex unary_branch_id;
    symbol_entry *found_symbol;

    for (auto task: tasks) {
        current_comptime_prerequisites.clear();

        // for each we want to know what symbols are referenced in this scope
        // of those we want to know which ones are pending comptime directives
        stack<NodeIndex> fringe;
        fringe.push(task.associated_ast_root);
        while (!fringe.empty()) {
            current_ast_index = fringe.top();
            current_identifier = node_man->get_identifier(current_ast_index);
            current_scope_id = node_man->get_scope_id(current_ast_index);
            fringe.pop();
            switch (node_man->get_host_nodetype(current_ast_index)) {
                case PN_UNARY: {
                    nodetype = node_man->get_nodetype(current_ast_index);
                    if (nodetype == PN_VAR_REF || nodetype == PN_IDENTIFIER) {
                        found_symbol = symbol_table.lookup(current_identifier, current_scope_id);
                        if (found_symbol->is_comptime_constant) current_comptime_prerequisites.insert(found_symbol->symbol_id);
                        continue;
                    }

                    unary_branch_id = node_man->get_branch(current_ast_index);
                    if (unary_branch_id == NULL_INDEX) continue;
                    fringe.push(unary_branch_id);
                }
                case PN_BINARY:
                case PN_TRINARY:
                    fringe.push(node_man->get_right(current_ast_index));
                    continue;
                case PN_BLOCK:
                    for (auto child_ast_index: node_man->get_children(current_ast_index)) {
                        fringe.push(child_ast_index);
                    }
                default:
                    continue;
            }
        }

        task.prerequisite_tasks.insert(task.prerequisite_tasks.end(), current_comptime_prerequisites.begin(), current_comptime_prerequisites.end());
    }
}

void ComptimeManager::execute_task(ComptimeTask *task) {
    SageInterpreter interpreter = SageInterpreter();
    interpreter.open(task->procedure_to_instruction_index, 4000);
    interpreter.load_program(task->task_instructions);
    interpreter.execute();
    task->symbol_injection_value = interpreter.get_return_value();
    interpreter.close();
    tasks_completed.fetch_add(1);
    queue_condition_variable.notify_all();
}

void ComptimeManager::execute_tasks_in_parallel(int thread_count) {
    tasks_completed = 0;
    int batch_size = staged_for_execution.size();
    vector<thread> workers;
    for (int i = 0; i < thread_count; ++i) {
        auto worker_function = [this, batch_size]() {
            while (tasks_completed < batch_size && !ErrorLogger::get().has_errors()) {
                ComptimeTask *task;
                {
                    unique_lock<mutex> lock(queue_mutex);
                    queue_condition_variable.wait(lock, [&]() {
                        return !staged_for_execution.empty() ||
                               tasks_completed >= batch_size ||
                               ErrorLogger::get().has_errors();
                    });
                    if (staged_for_execution.empty()) continue;
                    task = staged_for_execution.front();
                    staged_for_execution.pop();
                }
                execute_task(task);
            }
        };

        workers.emplace_back(worker_function);
    }

    for (auto &worker: workers) worker.join();
}

bool ComptimeManager::modifies_runtime_ast() {
    return modifies_runtime_ast_lock;
}

int ComptimeManager::get_next_task_prerequisite_count() {
    if (task_min_heap.empty()) return 0;
    return (int)task_min_heap.top()->prerequisite_tasks.size();
}

bool ComptimeManager::verify_comptime_dependencies() {
    auto &logger = ErrorLogger::get();

    stack<comptime_task_id> fringe;
    set<comptime_task_id> explored;
    comptime_task_id current_task_id;

    for (auto task: tasks) {
        if (task.prerequisite_tasks.empty()) fringe.push(task.task_id);
    }

    while (!fringe.empty()) {
        current_task_id = fringe.top();
        fringe.pop();

        if (explored.find(current_task_id) != explored.end()) {
            auto ast_index = tasks[current_task_id].associated_ast_root;
            auto token = node_man->get_token(ast_index);
            logger.log_error_unsafe(
                token,
                sen("Compile time directive has a circular dependency to another compile time directive."),
                ErrorType::COMPTIME);
            continue;
        }
        explored.insert(current_task_id);

        for (auto task_id: tasks[current_task_id].prerequisite_tasks) {
            fringe.push(task_id);
        }
    }

    return !logger.has_errors();
}






