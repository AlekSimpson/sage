#include <set>
#include <stack>
#include <numeric>
#include <cstring>
#include <atomic>
#include <algorithm>

#include "../include/analyzer.h"
#include "../include/parse_node.h"
#include "../include/node_manager.h"
#include "../include/cfg.h"

std::atomic<uint64_t> IDGenerator::next_id{0};

#define ui64 uint64_t

// Supporting Structures

VariableLifetime::VariableLifetime(ui64 id, string name, int start, bool priority) 
: id(id), name(name), start_line(start), end_line(start), high_priority(priority), register_assignment(-1) {}

VariableLifetime::VariableLifetime(string name, int start, bool priority) 
: name(name), start_line(start), end_line(start), high_priority(priority), register_assignment(-1) {
    id = IDGenerator::generate();
}

VariableLifetime::VariableLifetime() 
    : id(IDGenerator::generate()), name(""), start_line(0), end_line(0), 
      high_priority(false), register_assignment(-1), spill_offset(0), spilled(false) {}

void VariableLifetime::print_self() const {
    printf("VarLifetime{%ld, %s, %d, %d, reg: %d, offset: %d, spilled: %d}\n", id, name.c_str(), start_line, end_line, high_priority, register_assignment, spill_offset);
}

CFGNode::CFGNode(NodeIndex node, vector<string> references, InstructionType inst_type) 
: id(IDGenerator::generate()), node(node), variable_references(references), inst_type(inst_type) {}

CFGNode::CFGNode(NodeIndex node, vector<string> references, InstructionType inst_type, int line_no) 
: id(IDGenerator::generate()), line_number(line_no), node(node), variable_references(references), inst_type(inst_type) {}

CFGNode::CFGNode(ui64 id, NodeIndex node, vector<string> references, InstructionType inst_type) 
: id(id), node(node), variable_references(references), inst_type(inst_type) {}

CFGNode::CFGNode(const CFGNode& node) 
: id(IDGenerator::generate()), node(node.node), variable_references(node.variable_references), inst_type(node.inst_type) {}

CFGNode::CFGNode() 
    : id(IDGenerator::generate()), line_number(0), node(NULL_INDEX), 
      variable_references(), inst_type(INERT) {}

SageControlFlow::SageControlFlow() {
    // create start node
    ui64 temp_id = IDGenerator::generate();
    nodes[temp_id] = CFGNode(NULL_INDEX, vector<string>(), START_NODE);
    cursor = temp_id;
    start_node = temp_id; // should always have the starting node saved
}

// void SageControlFlow::add_connection(ui64 parent, CFGNode child) {
void SageControlFlow::add_new_node_connected(CFGNode child) {
    connections[cursor].push_back(child.id);
    nodes[child.id] = child;
    id_last_added = child.id;
    cursor = id_last_added;
}

void SageControlFlow::append_nested_flow(SageControlFlow* nested_cfg) {
    ui64 entrance_id = nested_cfg->connections[nested_cfg->start_node][0];
    CFGNode nested_entrance = nested_cfg->nodes[entrance_id];

    nested_cfg->nodes.erase(nested_cfg->start_node);
    nested_cfg->connections.erase(nested_cfg->start_node);

    nodes.merge(nested_cfg->nodes);
    connections.merge(nested_cfg->connections);

    add_connection(cursor, entrance_id);
    cursor = nested_cfg->cursor;
    delete nested_cfg;
}

void SageControlFlow::add_connection(uint64_t parent, uint64_t child) {
    connections[parent].push_back(child);
}

vector<uint64_t> SageControlFlow::get_dangling_cursors() {
    vector<uint64_t> dangling;
    
    // Find nodes that don't have outgoing connections (leaf nodes)
    for (const auto& [node_id, node] : nodes) {
        if (connections.find(node_id) == connections.end() || connections[node_id].empty()) {
            dangling.push_back(node_id);
        }
    }
    
    return dangling;
}

// Sage Analyzer Begin

SageAnalyzer::SageAnalyzer() {}

SageAnalyzer::SageAnalyzer(NodeManager* manager) {
    this->manager = manager;
    register_range_begin = 10;
    register_range_end = 21;
}

vector<string> SageAnalyzer::expression_contains_references(NodeIndex expression_root) {
    vector<string> return_value;
    vector<string> temp_one;
    vector<string> temp_two;
    vector<string> temp_three;

    if (expression_root == -1) {
        return return_value;
    }

    switch (manager->get_host_nodetype(expression_root)) {
        case PN_TRINARY:
            temp_one = expression_contains_references(manager->get_left(expression_root));
            return_value.insert(return_value.begin(), temp_one.begin(), temp_one.end());

            temp_two = expression_contains_references(manager->get_middle(expression_root));
            return_value.insert(return_value.begin(), temp_two.begin(), temp_two.end());

            temp_three = expression_contains_references(manager->get_right(expression_root));
            return_value.insert(return_value.begin(), temp_three.begin(), temp_three.end());
            break;

        case PN_BINARY:
            temp_one = expression_contains_references(manager->get_left(expression_root));
            return_value.insert(return_value.begin(), temp_one.begin(), temp_one.end());

            temp_two = expression_contains_references(manager->get_right(expression_root));
            return_value.insert(return_value.begin(), temp_two.begin(), temp_two.end());
            break;

        case PN_UNARY: {
            if (manager->get_nodetype(expression_root) == PN_VAR_REF) {
                return_value.push_back(manager->get_lexeme(expression_root));
                return return_value;
            }
            auto branch = manager->get_branch(expression_root);
            if (branch == -1) {
                return return_value;
            }

            temp_one = expression_contains_references(branch);
            return_value.insert(return_value.begin(), temp_one.begin(), temp_one.end());
            break;
        }

        default:
            // ERROR!!
            return return_value;
    }

    return return_value;
}


SageControlFlow* SageAnalyzer::generate_control_flow(NodeIndex ast_pointer) {
    auto nodetype = manager->get_nodetype(ast_pointer);
    if (nodetype != PN_BLOCK) {
        return nullptr;
    }

    SageControlFlow* cfg = new SageControlFlow();
    SageControlFlow* nested_flow;
    int relative_line_index = 0;
    NodeIndex current_line;
    vector<string> references;
    vector<ui64> dangling_cursors;
    vector<NodeIndex> lines = manager->get_children(ast_pointer);
    vector<string> found_references;
    vector<ui64> nested_dangling;
    NodeIndex inner_block;
    int current_line_num;

    // get line from block
    current_line = lines[relative_line_index];
    while (relative_line_index < lines.size()) {
        current_line_num = manager->get_token(current_line).linenum;

        switch (manager->get_nodetype(current_line)) {
        case PN_VAR_DEC: {
            references.push_back(manager->get_token(manager->get_left(current_line)).lexeme);
            cfg->add_new_node_connected(CFGNode(current_line, references, INIT, current_line_num));

            if (!dangling_cursors.empty()) {
                for (ui64 dangling_cursor : dangling_cursors) {
                    cfg->add_connection(dangling_cursor, cfg->cursor);
                }
                dangling_cursors.clear();
            }
            break;
        }
        case PN_ASSIGN: {
            references.push_back(manager->get_lexeme(manager->get_left(current_line)));
            found_references = expression_contains_references(manager->get_right(current_line));
            if (!found_references.empty()) {
                references.insert(references.end(), found_references.begin(), found_references.end());
            }

            cfg->add_new_node_connected(CFGNode(current_line, references, REFERENCE, current_line_num));

            if (!dangling_cursors.empty()) {
                for (ui64 dangling_cursor : dangling_cursors) {
                    cfg->add_connection(dangling_cursor, cfg->cursor);
                }
                dangling_cursors.clear();
            }
            break;
        }
        case PN_IF: {
            // add if node
            references = expression_contains_references(manager->get_left(current_line));
            cfg->add_new_node_connected(CFGNode(current_line, references, REFERENCE, current_line_num));

            // handle if edges
            vector<int> branches = manager->get_children(current_line);
            int edge_count = branches.size();

            // iterate over every branch in the if-else chain
            for (int branch = 0; branch < edge_count; ++branch) {
                nested_flow = generate_control_flow(branches[branch]);
                nested_dangling = nested_flow->get_dangling_cursors();
                dangling_cursors.insert(dangling_cursors.begin(), nested_dangling.begin(), nested_dangling.end());
                cfg->append_nested_flow(nested_flow); // deletes nested_flow memory after append
            }

            // always ends with dangling cursors left dangling
            // if includes else then all dangling will connect to next node generated
            // if it doesn't include else then the current if cursor will be included in the dangling cursors
            if (manager->get_nodetype(branches[edge_count-1]) != PN_ELSE_BRANCH) {
                dangling_cursors.push_back(cfg->cursor);
            }
            break;
        }
        case PN_WHILE: {
            references = expression_contains_references(manager->get_left(current_line));
            cfg->add_new_node_connected(CFGNode(current_line, references, WHILE, current_line_num));

            nested_flow = generate_control_flow(manager->get_right(current_line));
            dangling_cursors = nested_flow->get_dangling_cursors();
            cfg->append_nested_flow(nested_flow);

            for (ui64 dangling : dangling_cursors) {
                cfg->add_connection(dangling, cfg->cursor);
            }
            dangling_cursors.clear();
            break;
        }
        case PN_FOR: {
            references = expression_contains_references(manager->get_left(current_line));
            cfg->add_new_node_connected(CFGNode(current_line, references, FOR, current_line_num));

            nested_flow = generate_control_flow(manager->get_right(current_line));
            dangling_cursors = nested_flow->get_dangling_cursors();
            cfg->append_nested_flow(nested_flow);

            for (ui64 dangling : dangling_cursors) {
                cfg->add_connection(dangling, cfg->cursor);
            }
            dangling_cursors.clear();
            break;
        }
        // dont actively affect generation
        // but could show up as a valid line
        case PN_KEYWORD: {
            if (manager->get_branch(current_line) == NULL_INDEX) {
                cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));
                break;
            }

            cfg->add_new_node_connected(CFGNode(current_line, references, REFERENCE, current_line_num));
            break;
        }
        case PN_FUNCDEF: {
            cfg->add_new_node_connected(CFGNode(current_line, references, REFERENCE, current_line_num));

            inner_block = manager->reach_right(current_line, 2);
            nested_flow = generate_control_flow(inner_block);
            if (nested_flow == nullptr) {
                break;
            }

            dangling_cursors = nested_flow->get_dangling_cursors();

            cfg->append_nested_flow(nested_flow);
            break;
        }
        case PN_FUNCCALL: {
            // get all parameter expressions
            NodeIndex expression_block = manager->get_branch(current_line);
            vector<NodeIndex> parameter_expressions = manager->get_children(expression_block);
            // collect all expression_contains_references into one reference
            for (auto expression : parameter_expressions) {
                found_references = expression_contains_references(expression);
                references.insert(references.begin(), found_references.begin(), found_references.end());
            }
            // if no references collected, return INERT
            if (references.empty()) {
                cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));
                break;
            }
            // else return REFERENCE
            cfg->add_new_node_connected(CFGNode(current_line, references, REFERENCE, current_line_num));
            break;
        }
        case PN_STRUCT: {
            // TODO: when structs get the ability to set default attribute values we probably will have to change this logic
            cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));
            break;
        }
        case PN_RUN_DIRECTIVE: {
            cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));

            inner_block = manager->get_branch(current_line);
            nested_flow = generate_control_flow(inner_block);

            dangling_cursors = nested_flow->get_dangling_cursors();

            cfg->append_nested_flow(nested_flow);
            break;
        }
        case PN_INCLUDE: {
            cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));
            break;
        }
        case PN_FUNCDEC: {
            cfg->add_new_node_connected(CFGNode(current_line, references, INERT, current_line_num));
            break;
        }
        default:
            break;
        }

        relative_line_index++;
        current_line = lines[relative_line_index];
        references.clear();
    }

    cfg->greatest_line_number = current_line_num;
    return cfg;
}

unordered_map<string, VariableLifetime> SageAnalyzer::extract_lifetimes(SageControlFlow* controlflow) {
    if (controlflow == nullptr) {
        return unordered_map<string, VariableLifetime>();
    }

    set<ui64> explored;
    ui64 current_id = controlflow->start_node;
    vector<ui64> neighbors;
    vector<string> references;
    CFGNode current_node;
    unordered_map<string, VariableLifetime> retval;
    bool is_high_priority;
    stack<ui64> fringe;
    fringe.push(current_id);

    // traverse control flow
    while (!fringe.empty()) {
        current_id = fringe.top();
        fringe.pop();
        current_node = controlflow->nodes[current_id];
        is_high_priority = (current_node.inst_type == FOR || current_node.inst_type == WHILE);

        explored.insert(current_id);

        if (current_node.inst_type == START_NODE) {
            // skip past the start node
            current_id = controlflow->connections[current_id][0]; // start nodes will only ever have one connection
            neighbors = controlflow->connections[current_id];
            for (auto neighbor : neighbors) {
                if (explored.count(neighbor))
                    continue;

                fringe.push(neighbor);
            }
            continue;
        }

        // read variable references along the way
        references = current_node.variable_references;
        for (string reference: references) {
            if (retval.count(reference)) {
                // if we have already found this variable then we will continue marking the 
                // last used line number until we are finished traversing.
                // if we traverse the graph correctly we should end with the last reference to the variable 
                // and get its full lifetime
                retval[reference].end_line = current_node.line_number;
                continue;
            }

            // update table with variable references that points to their lifetime
            retval[reference] = VariableLifetime(reference, current_node.line_number, is_high_priority);
        }
        
        neighbors = controlflow->connections[current_id];
        for (auto neighbor : neighbors) {
            if (explored.count(neighbor))
                continue;

            fringe.push(neighbor);
        }
    }


    return retval;
}

void SageAnalyzer::linear_scan_register_allocation(int register_count) {
    struct ActiveAssignment {
        int active_register;
        int end_line;

        bool operator==(const ActiveAssignment& other) const {
            return active_register == other.active_register && end_line == other.end_line;
        }
    };

    auto fill_ascending = [](int start, int end) {
        set<int> value(end - start + 1);
        iota(value.begin(), value.end(), start);
        return value;
    }

    auto sort_by_startline = [](const unordered_map<string, VariableLifetime>& map) {
        vector<string> keys;
        keys.reserve(map.size());

        for (const auto& [key,value] : map) {
            keys.push_back(key);
        }

        sort(keys.begin(), keys.end(), [&map](const string& a, const string& b) {
            return map.at(a).start_line < map.at(b).start_line;
        });

        return keys;
    }

    vector<ActiveAssignment> active_registers;
    set<int> free_registers = fill_ascending(register_range_begin, register_range_end-1);

    


}

void SageAnalyzer::linear_scan_register_allocation(
    unordered_map<string, VariableLifetime>& intervals, int register_count
) {
    struct ActiveAssignment {
        int active_register;
        int end_line;

        bool operator==(const ActiveAssignment& other) const {
            return active_register == other.active_register && end_line == other.end_line;
        }
    };

    vector<ActiveAssignment> active_registers;
    vector<int> free_registers = fill_ascending(register_range_begin, register_range_end-1);

    vector<string> sorted_intervals = sort_keys_by_start(intervals);
    vector<string> expired_intervals;

    int free_register;
    vector<ActiveAssignment> temp_active;
    ActiveAssignment spill_candidate;
    for (string interval : sorted_intervals) {
        temp_active = active_registers;

        // expire the old intervals
        for (auto active_interval : temp_active) {
            if (active_interval.end_line >= intervals.at(interval).end_line)
                continue; // if active interval ending is well after the current ending interval

            active_registers.erase(
                std::remove(
                    active_registers.begin(), 
                    active_registers.end(), 
                    active_interval
                ), 
                active_registers.end()
            );
            free_registers.push_back(active_interval.active_register);
            sort(free_registers.begin(), free_registers.end());
        }

        if (active_registers.size() == register_count) {
            // spill register onto the stack
            spill_candidate = active_registers.at(active_registers.size()-1);

            if (spill_candidate.end_line > intervals.at(interval).end_line) {
                intervals.at(interval).register_assignment = spill_candidate.active_register;
                /*spill_candidate.spilled = true;*/

                active_registers.erase(
                    std::remove(
                        active_registers.begin(), 
                        active_registers.end(), 
                        spill_candidate
                    ), 
                    active_registers.end()
                );
                active_registers.push_back(ActiveAssignment{intervals.at(interval).register_assignment, intervals.at(interval).end_line});

                sort(active_registers.begin(), active_registers.end(), [](const ActiveAssignment& a, const ActiveAssignment& b) {
                    return a.end_line < b.end_line;
                });

            } else {
                intervals.at(interval).spilled = true;
            }
 
            continue;
        }

        // otherwise assign a free register
        free_register = free_registers[0];
        free_registers.erase(free_registers.begin());

        intervals.at(interval).register_assignment = free_register;
        active_registers.push_back(ActiveAssignment{free_register, INF});

        sort(active_registers.begin(), active_registers.end(), [](const ActiveAssignment& a, const ActiveAssignment& b) {
            return a.end_line < b.end_line;
        });
    }
}

void SageAnalyzer::perform_static_analysis(NodeIndex root) {
    auto controlflow = generate_control_flow(root); // heap memory
    // unordered_map<string, VariableLifetime> lifetimes = extract_lifetimes(controlflow);
    variable_lifetimes = extract_lifetimes(controlflow);

    linear_scan_register_allocation(variable_lifetimes, 10);

    delete controlflow;
    // variable_lifetimes = lifetimes;
}




























