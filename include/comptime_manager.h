#pragma once

#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "node_manager.h"
#include "sage_bytecode.h"

typedef int comptime_task_id ;

enum class TaskResultType {
    VALUE,
    STRING_INJECTION,
    AST_INSERTION
};

struct ComptimeTask {
    bytecode task_instructions;
    vector<comptime_task_id> prerequisite_tasks;
    map<int, int> procedure_to_instruction_index;

    string injection_string_result;

    comptime_task_id task_id = -1;
    int associated_scope_id = -1;
    NodeIndex associated_ast_root = -1;
    TaskResultType result_type = TaskResultType::VALUE;
    SageValue symbol_injection_value;
};

struct TaskComparator {
    bool operator()(const ComptimeTask &a, const ComptimeTask &b) {
        return a.prerequisite_tasks.size() > b.prerequisite_tasks.size();
    }
};

struct ComptimeManager {
    priority_queue<ComptimeTask, vector<ComptimeTask>, TaskComparator> tasks;
    map<NodeIndex, comptime_task_id> node_to_task_id;

    mutex queue_mutex;
    condition_variable queue_condition;
    atomic<int> tasks_completed{0};

    int execution_iterations = 0;
    static constexpr int MAX_ITERATIONS = 10;

    ComptimeManager();

    void add_task(ComptimeTask task);
    void execute_tasks_in_parallel(int thread_count);
    bool modifies_runtime_ast();
};















