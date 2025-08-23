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

u64 DependencyGraph::add_node(string name, dep_type type, NodeIndex ast_pos) {
    if (nodename_map.find(name) == nodename_map.end()) {
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
        return id;
    }

    local_scope.insert(name);

    IdentNode info = IdentNode{name, type, ast_pos, 0, nullptr};
    int new_id = info.get_id();
    nodes[new_id] = info;
    connections[new_id] = set<u64>();
    nodename_map[name] = new_id;
    return new_id;
}

u64 DependencyGraph::add_scope_node(string name, dep_type type, NodeIndex ast_pos, DependencyGraph* owned_scope) {
    if (nodename_map.find(name) == nodename_map.end()) {
        auto id = nodename_map[name];
        // if we are initializing a new node and the existing node is not just a reference
        if (type == INI && nodes[id].type == INI) {
            auto token = man->get_token(ast_pos);
            ErrorLogger::get().log_error(
                token,
                str("redefinition of ", name, " is not allowed"), 
                GENERAL);
        }

        local_scope.insert(name);

        nodes[id].type = type;
        nodes[id].ast_pos = ast_pos;
        nodes[id].owned_scope = owned_scope;

        return id;
    }

    IdentNode info = IdentNode{name, type, ast_pos, 0, owned_scope};
    int new_id = info.get_id();
    nodes[new_id] = info;
    connections[new_id] = set<u64>();
    nodename_map[name] = new_id;
    if (owned_scope->scope_level < 0) {
        comptime_nodes.insert(new_id);
    }

    return new_id;
}

void DependencyGraph::add_connection(const string& dependency, const string& dependent) {
    // "a depends on b" (b --> a) where a is dependent and b is dependency

    // dependency and dependent may or may not have already been parsed when we add a connection for either of them
    if (nodename_map.find(dependency) == nodename_map.end()) {
        add_node(dependency, REF, -1);
    }
    if (nodename_map.find(dependent) == nodename_map.end()) {
        add_node(dependent, REF, -1);
    }

    auto dependency_id = nodename_map[dependency];
    auto dependent_id = nodename_map[dependent];

    connections[dependency_id].insert(dependent_id);
    nodes[dependency_id].degree++;
    nodes[dependent_id].degree++;
}

set<u64>& DependencyGraph::get_dependents(const u64 node) {
    return connections[node];
}

void DependencyGraph::load_fringe(stack<u64>& fringe) {
    for (auto& [id, node]: nodes) {
        if (get_in_degree(id) == 0) {
            fringe.push(id);
        }
    }
}

int DependencyGraph::get_in_degree(const u64 id) {
    auto in_degree = nodes[id].degree - get_dependents(id).size(); // degree - out_degree
    return in_degree;
}

bool DependencyGraph::dependencies_are_valid() {
    for (const auto& [key, value] : nodes) {
        // all in degree 0 nodes are of type INI
        if (get_in_degree(key) == 0 && value.type == REF) {
            auto token = man->get_token(value.ast_pos);
            ErrorLogger::get().log_error(
                token,
                str("undefined reference, ", value.name),
                GENERAL);
        }
    }

    // no circular dependencies
    set<u64> explored;
    stack<u64> fringe;
    u64 walk;

    load_fringe(fringe);
    while (explored.size() < nodes.size()) {
        walk = fringe.top();
        fringe.pop();

        explored.insert(walk);

        set<u64>& neighbors = get_dependents(walk);
        for (u64 id : neighbors) {
            if (explored.find(id) != explored.end()) {
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

    return ErrorLogger::get().has_errors();
}

void DependencyGraph::merge_with(DependencyGraph& subscope) {
    set<u64> explored;
    stack<u64> fringe;
    u64 walk;
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
                add_connection(walknode.name, nodevalue.name);
            }
            nodes[nodename_map[walknode.name]].merge(walknode);
        }else {
            add_node(walknode.name, walknode.type, walknode.ast_pos);
        }

        for (u64 id : walk_dependents) {
            if (explored.find(id) != explored.end()) {
                continue;
            }
            fringe.push(id);
        }
    }
}

vector<u64> DependencyGraph::get_exec_order() {
    ascending_list<u64> order = ascending_list<u64>(nodes.size());
    set<u64> explored;
    stack<u64> indeg_zero;
    queue<u64> fringe;
    u64 walk;
    
    for (auto& [id, node] : nodes) {
        if (get_in_degree(id) == 0) {
            indeg_zero.push(id);
        }
    }

    auto node_sorter = [this](u64 id) {
        return this->get_in_degree(id);
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

        set<u64>& neighbors = get_dependents(walk);
        for (u64 id : neighbors) {
            if (explored.find(id) != neighbors.end()) {
                continue;
            }
            fringe.push(id);
        }
    }

    return order.to_vector();
}

int DependencyGraph::partition(vector<u64>* buffer, int low, int high) {
    u64 pivot = (*buffer)[high];
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

void DependencyGraph::quicksort(vector<u64>* buffer, int low, int high) {
    if (low < high) {
        int pivot_index = partition(buffer, low, high);
        quicksort(buffer, low, pivot_index - 1);
        quicksort(buffer, pivot_index + 1, high);
    }
}

void DependencyGraph::quicksort(vector<u64>* buffer) {
    quicksort(buffer, 0, buffer->size()-1);
}

u64 IdentNode::get_id() {
    return reinterpret_cast<u64>(this);
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

















