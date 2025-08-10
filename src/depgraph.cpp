#include "../include/depgraph.h"
#include <map>
#include <string>

DependencyGraph::DependencyGraph() : scopename("none"), scope_level(0) {}
DependencyGraph::DependencyGraph(string scopename, int scope_level) : scopename(scopename), scope_level(scope_level), errors_exist(false) {}

u64 DependencyGraph::add_node(string name, dep_type type, NodeIndex ast_pos) {
    if (nodename_map.find(name) == nodename_map.end()) {
        auto id = nodename_map[name];
        if (type == INIT && nodes[id].dep_type == INIT) {
            errors_exist = true;
            // ERROR!! LOG ERROR
        }

        nodes[id].dep_type = type;
        nodes[id].ast_pos = ast_pos;
        return id;
    }

    IdentInfo info = IdentInfo{name, type, ast_pos, 0};
    int new_id = info.get_id();
    nodes[new_id] = info;
    connections[new_id] = set<u64>();
    nodename_map[name] = new_id;
    return new_id;
}

void DependencyGraph::add_connection(string dependency, string dependent) {
    // "a depends on b" (b --> a) where a is dependent and b is dependency
    if (nodename_map.find(dependency) == nodename_map.end()) {
        add_node(dependency, REF, -1);
    }

    connections[dependency].insert(dependent);
    nodes[dependency].degree++;
    nodes[dependent].degree++;
}

set<u64>& DependencyGraph::get_dependents(u64 node) {
    return connections[node];
}

void DependencyGraph::load_fringe(stack<u64>& fringe) {
    for (u64 id : nodes) {
        if (get_in_degree(id) == 0) {
            fringe.push(id);
        }
    }
}

int DependencyGraph::get_in_degree(u64 id) {
    auto in_degree = nodes[id].degree - get_dependents(id).size(); // degree - out_degree
    return in_degree
}

bool DependencyGraph::dependencies_are_valid() {
    // TODO: add errors so that we can report errors here
    bool result = true;

    if (errors_exist) {
        // an initialization of the same variable occurred twice
        return false;
    }

    for (const auto& [key, value] : graph) {
        // all in degree 0 nodes are of type INIT
        if (get_in_degree(key) == 0 && value.type == REF) {
            // ERROR!!
            if (result) {
                result = false;
            }
        }
    }

    // no circular dependencies
    set<u64> explored;
    stack<u64> fringe;
    u64 current_id;

    load_fringe(fringe);
    while (explored.size() < nodes.size()) {
        current_id = fringe.top();
        fringe.pop();

        explored.push_back(current_id);

        set<u64>& neighbors = get_dependents(current_id);
        for (u64 id : neighbors) {
            if (explored.find(id) != explored.end()) {
                // FOUND A CYCLE
                // ERROR!!
                if (result) {
                    result = false;
                }
                continue;
            }
            fringe.push(id);
        }
    }

    return result;
}

void DependencyGraph::merge_with(DependencyGraph& subscope) {
    set<u64> explored;
    stack<u64> fringe;
    u64 walk;
    IdentNode walkname;

    subscope.load_fringe(fringe);
    while (fringe.size() > 0) {
        walk = fringe.top();
        fringe.pop();

        explored.push_back(walk);

        // does walk have a doppleganger?
        walknode = subscope.nodes[walk];
        auto walk_dependents = subscope.get_dependents(walk);
        if (nodename_map.find(walknode.name) != nodename_map.end()) {
            for (auto dependent : walk_dependents) {
                add_connection(walknode.name, dependent);
            }
        }else {
            add_node(walknode.name, walknode.dep_type, walknode.ast_pos, walknode.degree);
        }

        for (u64 id : walk_dependents) {
            if (explored.find(id) != explored.end()) {
                continue;
            }
            fringe.push(id);
        }
    }
}

vector<u64> DependencyGraph::comptime_exec_order() {
    vector<u64> order;
    set<u64> explored;
    stack<u64> indeg_zero;
    queue<u64> fringe;
    u64 walk;

    auto sorted_insert = [this, order](u64 value) {
        auto it = std::lower_bound(order.begin(), order.end(), get_in_degree(value));
        order.insert(it, value);
    };
    
    for (auto node : nodes) {
        if (get_in_degree(node) == 0) {
            indeg_zero.push(node);
        }
    }

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

        sorted_insert(value);

        set<u64>& neighbors = get_dependents(walk);
        for (u64 id : neighbors) {
            if (explored.find(id) != neighbors.end()) {
                continue;
            }
            fringe.push(id);
        }
    }

    return order;
}


u64 IdentNode::get_id() {
    return reinterpret_cast<u64>(this);
}

















