#include <vector>
#include <string>
#include "../include/parse_node.h"

NodeManager::NodeManager() {
	box_count = 0;
	capacity = 500;
	conatiner = new nodebox[500];
	for (int i = 0; i < 500; i++) {
		container[i] = {nullptr, 0};
		free_spaces.push_back(i);
	}

    root_node = -1;
}

NodeManager::~NodeManager() {
	delete[] conatiner;
}

nodebox NodeManager::get_node(NodeIndex node) {
	if (node < 0 || node > container.size()) {
		printf("WARNING: accessing invalid node index %d\n", node);
		return nullptr;
	}

	nodebox box = container[node];
	return box;
}

string NodeManager::to_string(NodeIndex index) {
	auto node = get_node(index).node;
	if (node == nullptr) {
		return;
	}

    return node->to_string();
}

void Node::Manager::showtree(NodeIndex index) {
	auto node = get_node(index).node;
	if (node == nullptr) {
		return;
	}

    node->showtree("");
}

ParseNodeType NodeManager::get_nodetype(NodeIndex index) {
	auto node = get_node(index).node;
	if (node == nullptr) {
		return;
	}
	return node->rep_nodetype;
}

ParseNodeType NodeManager::get_host_nodetype(NodeIndex index) {
	auto node = get_node(index).node;
	if (node == nullptr) {
		return;
	}
	return node->host_nodetype;
}

NodeIndex NodeManager::get_left(NodeIndex index) {
	auto box = get_node(index);
	if 

	auto node = box.node;
	auto hosttype = box.host_type;	

	if (hosttype == PN_BINARY) {
		BinaryParseNode* binary_node = dyanmic_cast<BinaryParseNode*>(node);
		return binary_node->left;

	}else if (hosttype == PN_TRINARY) {
		TrinaryParseNode* trinary_node = dyanmic_cast<TrinaryParseNode*>(node);
		return trinary_node->left;
	}
	
	return -1;
}

NodeIndex NodeManager::get_right(NodeIndex node) {
	if (node < 0 || node > container.size()) {
        printf("WARNING | get_right: OUT OF RANGE NODE INDEX GIVE %d\n", node);
		return 0;
	}

	auto box = conatiner.at_node(node);
	auto node = box.node;
	auto hosttype = box.host_type;	

	if (hosttype == PN_BINARY) {
		BinaryParseNode* binary_node = dyanmic_cast<BinaryParseNode*>(node);
		return binary_node->right;

	}else if (hosttype == PN_TRINARY) {
		TrinaryParseNode* trinary_node = dyanmic_cast<TrinaryParseNode*>(node);
		return trinary_node->right;
	}
	
	return -1;
}

NodeIndex NodeManager::get_middle(NodeIndex node) {
	if (node < 0 || node > container.size()) {
        printf("WARNING | get_middle: OUT OF RANGE NODE INDEX GIVE %d\n", node);
		return 0;
	}

	auto box = conatiner.at_node(node);
	auto node = box.node;
	auto hosttype = box.host_type;	

	if (hosttype == PN_TRINARY) {
		TrinaryParseNode* trinary_node = dyanmic_cast<TrinaryParseNode*>(node);
		return trinary_node->middle;
	}
	
	return -1;
}

NodeIndex NodeManager::get_branch(NodeIndex node) {
	if (node < 0 || node > container.size()) {
        printf("WARNING | get_branch: OUT OF RANGE NODE INDEX GIVE %d\n", node);
		return 0;
	}

	auto box = conatiner.at_node(node);
	auto node = box.node;
	auto hosttype = box.host_type;	

	if (hosttype == PN_UNARY) {
		UnaryParseNode* unary_node = dyanmic_cast<UnaryParseNode*>(node);
		return unary_node->branch;
	}
	
	return -1;
}

vector<NodeIndex> NodeManager::get_children(NodeIndex node) {
	if (node < 0 || node > container.size()) {
        printf("WARNING | get_children: OUT OF RANGE NODE INDEX GIVE %d\n", node);
		return 0;
	}

	auto box = conatiner.at_node(node);
	auto node = box.node;
	auto hosttype = box.host_type;	

	if (hosttype == PN_BLOCk) {
		BlockParseNode* block_node = dyanmic_cast<BlockParseNode*>(node);
		return block_node->branch;
	}
	
	return -1;
}

void NodeManager::delete_node(NodeIndex index) {
	delete conatiner[index].node;
	container[index].node = nullptr;

	free_spaces.push_back(index);

	// int i;
	// for (i = 0; i < free_spaces.size(); i++) {
	// 	if (free_spaces.at(i) == index) {
	// 		break;
	// 	}
	// }
	// free_spaces.erase(free_spaces.begin() + i);
}

NodeIndex NodeManager::create_block() {}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type) {}

NodeIndex NodeManager::create_block(Token token, ParseNodeType type, vector<NodeIndex> nodes) {}

NodeIndex NodeManager::create_binary(Token token, ParseNodeType type, NodeIndex left, NodeIndex right) {}

NodeIndex NodeManager::create_trinary(Token token, ParseNodeType type, NodeIndex left, NodeIndex middle, NodeIndex right) {}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type) {}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, NodeIndex branch) {}

NodeIndex NodeManager::create_unary(Token token, ParseNodeType type, vector<string> lexemes) {}

void NodeManager::expand_container() {
	int old_cap = capactiy;
	capacity = capacity * 2;

	nodebox* new_container = new nodebox[capacity];

	for (int i = 0; i < old_cap; i++) {
		new_container[i] = container[i];
	}

	delete[] container;
	container = nullptr;

	container = new_container;
}




