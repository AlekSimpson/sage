#include "../include/depgraph.h"
#include "../include/ascending_list.h"
#include "../include/error_logger.h"
#include <map>
#include <string>
#include <set>
#include <stack>
#include <queue>

DependencyGraph::DependencyGraph() : man(nullptr), parent_scope(nullptr), scopename("none"), scope_level(0), errors_exist(false) {}

DependencyGraph::DependencyGraph(NodeManager* man, string scopename, int scope_level, set<string>* parent_scope)
: man(man), parent_scope(parent_scope), scopename(scopename), scope_level(scope_level), errors_exist(false) {}

DependencyGraph::~DependencyGraph() {
    for (auto [key, value] : nodes) {
        if (value.owned_scope != nullptr) {
            delete value.owned_scope;
        }
    }
}

int DependencyGraph::add_node(string name, dep_type type, NodeIndex ast_pos) {
    if (nodename_map.find(name) != nodename_map.end()) {
        auto id = nodename_map[name];
        if (type == INI && nodes[id].type == INI) {
            auto token = man->get_token(ast_pos);
            ErrorLogger::get().log_error(
                token,
                str("redefinition of ", name, " is not allowed"), 
                GENERAL);
        }

        nodes[id].type = type;
        nodes[id].ast_pos = ast_pos;
        for (auto dependent : get_dependents(id)) {
            // in degree doesn't count for connections from nodes that are references at the time of connection creation
            nodes[dependent].in_degree++;
        }
        return id;
    }

    local_scope.insert(name);

    int new_id = nodes.size();
    nodes[new_id] = IdentNode{name, type, ast_pos, 0, nullptr};
    connections[new_id] = set<int>();
    nodename_map[name] = new_id;
    return new_id;
}

int DependencyGraph::add_param_node(string name, dep_type type, NodeIndex ast_pos) {
     if (nodename_map.find(name) != nodename_map.end()) {
         auto id = nodename_map[name];
         if (type == INI && nodes[id].type == INI) {
             auto token = man->get_token(ast_pos);
             ErrorLogger::get().log_error(
                 token,
                 str("redefinition of ", name, " is not allowed"),
                 GENERAL);
         }
         if (type == REF) return id;

         nodes[id].type = type;
         nodes[id].ast_pos = ast_pos;
         nodes[id].is_parameter = true;
         for (auto dependent : get_dependents(id)) {
             // in degree doesn't count for connections from nodes that are references at the time of connection creation
             nodes[dependent].in_degree++;
         }
         return id;
    }

    local_scope.insert(name);

    int new_id = nodes.size();
    nodes[new_id] = IdentNode{name, type, ast_pos, 0, nullptr};
    nodes[new_id].is_parameter = true;
    connections[new_id] = set<int>();
    nodename_map[name] = new_id;
    return new_id;
}

int DependencyGraph::add_scope_node(string name, dep_type type, NodeIndex ast_pos, DependencyGraph* owned_scope) {
    if (nodename_map.find(name) != nodename_map.end()) {
        auto id = nodename_map[name];
        // if we are initializing a new node and the existing node is not just a reference
        if (type == INI && nodes[id].type == INI) {
            auto token = man->get_token(ast_pos);
            ErrorLogger::get().log_error(
                token,
                sen("redefinition of", name, "is not allowed"),
                GENERAL);
        }

        local_scope.insert(name);

        nodes[id].type = type;
        nodes[id].ast_pos = ast_pos;
        nodes[id].owned_scope = owned_scope;
        man->bind_dependency(ast_pos, owned_scope);
        for (auto dependent : get_dependents(id)) {
            // in degree doesn't count for connections from nodes that are references at the time of connection creation
            nodes[dependent].in_degree++;
        }

        return id;
    }


    local_scope.insert(name);

    int new_id = nodes.size();
    nodes[new_id] = IdentNode{name, type, ast_pos, 0, owned_scope};
    connections[new_id] = set<int>();
    nodename_map[name] = new_id;
    if (owned_scope->scope_level < 0) {
        comptime_nodes.insert(new_id);
    }
    man->bind_dependency(ast_pos, owned_scope);

    return new_id;
}

void DependencyGraph::add_connection(const string& dependency, const string& dependent, NodeIndex astpos) {
    // "a depends on b" (b --> a) where a is dependent and b is dependency

    // dependency and dependent may or may not have already been parsed when we add a connection for either of them
    if (nodename_map.find(dependency) == nodename_map.end()) {
        add_node(dependency, REF, astpos);
    }
    if (nodename_map.find(dependent) == nodename_map.end()) {
        add_node(dependent, REF, astpos);
    }

    auto dependency_id = nodename_map[dependency];
    auto dependent_id = nodename_map[dependent];

    connections[dependency_id].insert(dependent_id);
    if (nodes[dependency_id].type == INI) {
        // a node cannot depend on a reference
        nodes[dependent_id].in_degree++;
    }
}

set<int>& DependencyGraph::get_dependents(const int node) {
    return connections[node];
}

void DependencyGraph::load_fringe(stack<int>& fringe) {
    for (auto& [id, node]: nodes) {
        if (nodes[id].in_degree == 0) {
            fringe.push(id);
        }
    }
}

bool DependencyGraph::dependencies_are_valid() {
    for (const auto& [key, value] : nodes) {
        // all in degree 0 nodes are of type INI
        if (value.type == REF && (value.in_degree == 0 && parent_scope->find(value.name) == parent_scope->end())) {
            auto token = man->get_token(value.ast_pos);
            ErrorLogger::get().log_error(
                token,
                sen("undefined reference:", value.name),
                GENERAL);
        }
    }

    // no circular dependencies
    set<int> explored;
    stack<int> fringe;
    int walk;

    load_fringe(fringe);
    while (explored.size() < nodes.size()) {
        walk = fringe.top();
        fringe.pop();

        if (nodes[walk].owned_scope != nullptr) {
            nodes[walk].owned_scope->dependencies_are_valid();
        }
        explored.insert(walk);

        set<int>& neighbors = get_dependents(walk);
        for (int id : neighbors) {
            if (explored.find(id) != explored.end()) {
                if (nodes[id].type != INI || nodes[walk].type != INI) {
                    continue;
                }

                auto token = man->get_token(nodes[id].ast_pos);
                ErrorLogger::get().log_error(
                    token,
                    str(nodes[id].name, " is a circular defintion. Circular defintions are not allowed."),
                    GENERAL);
                continue;
            }
            fringe.push(id);
        }
    }

    return !ErrorLogger::get().has_errors();
}

void DependencyGraph::merge_with(DependencyGraph& subscope) {
    set<int> explored;
    stack<int> fringe;
    int walk;
    IdentNode walknode;

    subscope.load_fringe(fringe);
    while (fringe.size() > 0) {
        walk = fringe.top();
        fringe.pop();

        explored.insert(walk);

        // does walk have a doppelgänger?
        walknode = subscope.nodes[walk];
        auto walk_dependents = subscope.get_dependents(walk);
        if (nodename_map.find(walknode.name) != nodename_map.end()) {
            for (auto dependent : walk_dependents) {
                auto nodevalue = subscope.nodes[dependent];
                add_connection(walknode.name, nodevalue.name, 0);
            }
            nodes[nodename_map[walknode.name]].merge(walknode);
        }else {
            add_node(walknode.name, walknode.type, walknode.ast_pos);
        }

        for (int id : walk_dependents) {
            if (explored.find(id) != explored.end()) {
                continue;
            }
            fringe.push(id);
        }
    }
}

vector<int> DependencyGraph::get_exec_order() {
    ascending_list<int> order = ascending_list<int>(nodes.size());
    set<int> explored;
    stack<int> indeg_zero;
    queue<int> fringe;
    int walk;
    
    for (auto& [id, node] : nodes) {
        if (nodes[id ].in_degree == 0) {
            indeg_zero.push(id);
        }
    }

    auto node_sorter = [this](int id) {
        return this->nodes[id].in_degree;
    };

    while (explored.size() < nodes.size()) {
        if (fringe.size() == 0) {
            if (indeg_zero.size() == 0) {
                break;
            }

            fringe.push(indeg_zero.top());
            indeg_zero.pop();
        }

        walk = fringe.front();
        fringe.pop();

        order.insert(walk, node_sorter);

        set<int>& neighbors = get_dependents(walk);
        for (int id : neighbors) {
            if (explored.find(id) != neighbors.end()) {
                continue;
            }
            fringe.push(id);
        }
    }

    vector<int> result = order.to_vector();
    return result;
}

int DependencyGraph::partition(vector<int>* buffer, int low, int high) {
    int pivot = (*buffer)[high];
    int pivot_priority = nodes[pivot].get_sort_value();
    int i = low - 1;

    for (int j = low; j < high; ++j) {
        if (nodes[(*buffer)[j]].get_sort_value() <= pivot_priority) {
            i++;
            std::swap(buffer[i], buffer[j]);
        }
    }
    std::swap(buffer[i + 1], buffer[high]);
    return i + 1;
}

void DependencyGraph::quicksort(vector<int>* buffer, int low, int high) {
    if (low < high) {
        int pivot_index = partition(buffer, low, high);
        quicksort(buffer, low, pivot_index - 1);
        quicksort(buffer, pivot_index + 1, high);
    }
}

void DependencyGraph::quicksort(vector<int>* buffer) {
    quicksort(buffer, 0, buffer->size()-1);
}

void IdentNode::merge(IdentNode& node) {
    high_priority = node.high_priority;
    reference_count = reference_count + node.reference_count;
    if (type == REF && node.type == INI) {
        auto astnode = node.ast_pos;
        auto token = man->get_token(astnode);
        ErrorLogger::get().log_error(
            token,
            str("Variable (",node.name,") declared in child scope, referenced in parent scope."),
            GENERAL);
    }
}

int IdentNode::get_sort_value() {
    if (owned_scope != nullptr) {
        return -1;
    }
    if (sort_value != -1) {
        return sort_value;
    }

    sort_value = reference_count + (high_priority * 2) + (instruction_freq * 3);
    return sort_value;
}

















