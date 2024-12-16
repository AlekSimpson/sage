package main

import (
	"fmt"
)

type NodeType int

const (
	BINARY NodeType = iota
	TRINARY
	UNARY
	NUMBER
	IDENTIFIER
	KEYWORD
	BLOCK
	CODE_BLOCK
	PARAM_LIST
	FUNCDEF
	FUNCCALL
	TYPE
	STRUCT
	IF
	IF_BRANCH
	ELSE_BRANCH
	WHILE
	ASSIGN
	FOR
	PROGRAM
	RANGE
	VAR_DEC
	VAR_REF
)

type ParseNode interface {
	print() string
	showtree(depth string)
	get_token() *Token
	get_nodetype() NodeType
}

func nodetype_to_string(type_ NodeType) string {
	nodetype_map := map[NodeType]string{
		BINARY: "BINARY", NUMBER: "NUMBER", IDENTIFIER: "IDENTIFIER",
		KEYWORD: "KEYWORD", BLOCK: "BLOCK", CODE_BLOCK: "CODE_BLOCK", PARAM_LIST: "PARAM_LIST", FUNCDEF: "FUNCDEF", FUNCCALL: "FUNCCALL",
		TYPE: "TYPE", STRUCT: "STRUCT", IF: "IF", WHILE: "WHILE", FOR: "FOR", PROGRAM: "PROGRAM", VAR_DEC: "VAR_DEC",
	}
	return nodetype_map[type_]
}

//// BEGIN BLOCK NODE

type BlockNode struct {
	token    *Token
	nodetype NodeType
	children []ParseNode
}

func ModuleRootNode() *BlockNode {
	rootTok := &Token{
		lexeme:  "",
		linenum: 0,
	}

	return &BlockNode{
		token:    rootTok,
		nodetype: PROGRAM,
	}
}

func NewBlockNode(tok *Token, c []ParseNode) *BlockNode {
	return &BlockNode{
		token:    tok,
		children: c,
	}
}

func (n *BlockNode) get_nodetype() NodeType {
	return BLOCK
}

func (n *BlockNode) get_token() *Token {
	return n.token
}

func (n *BlockNode) print() string {
	return "BLOCK_NODE"
}

func (n *BlockNode) showtree(depth string) {
	fmt.Println(depth + "- " + n.print())

	for _, child := range n.children {
		child.showtree(depth + "\t")
	}
}

//// BEGIN BINARY NODE

type BinaryNode struct {
	token    *Token
	nodetype NodeType
	left     ParseNode
	right    ParseNode
}

func NewBinaryNode(t *Token, nt NodeType, l ParseNode, r ParseNode) *BinaryNode {
	return &BinaryNode{
		token:    t,
		nodetype: nt,
		left:     l,
		right:    r,
	}
}

func (n *BinaryNode) get_nodetype() NodeType {
	return BINARY
}

func (n *BinaryNode) get_token() *Token {
	return n.token
}

func (n *BinaryNode) print() string {
	return fmt.Sprintf("BINARY NODE (%s: %s | %s | %s)", nodetype_to_string(n.nodetype), n.token.lexeme, n.left.get_token().lexeme, n.right.get_token().lexeme)
}

func (n *BinaryNode) showtree(depth string) {
	fmt.Println(depth + "- " + n.print())

	if n.left != nil {
		n.left.showtree(depth + "\t")
	}
	if n.right != nil {
		n.right.showtree(depth + "\t")
	}
}

//// BEGIN TRINARY NODE

type TrinaryNode struct {
	token    *Token
	nodetype NodeType
	left     ParseNode
	middle   ParseNode
	right    ParseNode
}

func NewTrinaryNode(t *Token, nodetype NodeType, left ParseNode, middle ParseNode, right ParseNode) *TrinaryNode {
	return &TrinaryNode{
		token:    t,
		nodetype: nodetype,
		left:     left,
		middle:   middle,
		right:    right,
	}
}

func (n *TrinaryNode) get_nodetype() NodeType {
	return TRINARY
}

func (n *TrinaryNode) get_token() *Token {
	return n.token
}

func (n *TrinaryNode) print() string {
	return fmt.Sprintf("TRINARY NODE (%s: %s | %s | %s | %s)", nodetype_to_string(n.nodetype), n.token.lexeme, n.left.get_token().lexeme, n.middle.get_token().lexeme, n.right.get_token().lexeme)
}

func (n *TrinaryNode) showtree(depth string) {
	fmt.Println(depth + "- " + n.print())

	if n.left != nil {
		n.left.showtree(depth + "\t")
	}
	if n.middle != nil {
		n.middle.showtree(depth + "\t")
	}
	if n.right != nil {
		n.right.showtree(depth + "\t")
	}
}

//// BEGIN UNARY NODE

type UnaryNode struct {
	token    *Token
	nodetype NodeType
	node     ParseNode
	tag      string // author's message indicating extra meta information about node
}

func (n *UnaryNode) get_nodetype() NodeType {
	return UNARY
}

func (n *UnaryNode) print() string {
	return fmt.Sprintf("UNARY NODE (%s: %s | %s)", nodetype_to_string(n.nodetype), n.token.lexeme, n.node.get_token().lexeme)
}

func (n *UnaryNode) get_token() *Token {
	return n.token
}

func (n *UnaryNode) showtree(depth string) {
	fmt.Println(depth + "- " + n.print())

	if n.node != nil {
		n.node.showtree(depth + "\n")
	}
}

func (n *UnaryNode) addtag(message string) {
	n.tag = message
}

func NewUnaryNode(tok *Token, nodetype NodeType) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		nodetype: nodetype,
		tag:      "",
	}
}

func NewBranchUnaryNode(tok *Token, nodetype NodeType, branch_node ParseNode) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		nodetype: nodetype,
		node:     branch_node,
		tag:      "",
	}
}
