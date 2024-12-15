package main

import (
	"fmt"
)

type NodeType int

const (
	BINARY NodeType = iota
	NUMBER
	IDENTIFIER
	KEYWORD
	BLOCK
	CODE_BLOCK
	PARAM_LIST
	FUNC
	TYPE
	STRUCT
	IF
	WHILE
	FOR
	PROGRAM
	VAR_DEC
)

type ParseNode interface {
	print() string
	showtree(depth string)
	get_token() *Token
}

func nodetype_to_string(type_ NodeType) string {
	nodetype_map := map[NodeType]string{
		BINARY: "BINARY", NUMBER: "NUMBER", IDENTIFIER: "IDENTIFIER",
		KEYWORD: "KEYWORD", BLOCK: "BLOCK", CODE_BLOCK: "CODE_BLOCK", PARAM_LIST: "PARAM_LIST", FUNC: "FUNC",
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

//// BEGIN UNARY NODE

type UnaryNode struct {
	token    *Token
	nodetype NodeType
}

func (n *UnaryNode) print() string {
	return fmt.Sprintf("UnaryNode{%s}", n.token.show())
}

func (n *UnaryNode) get_token() *Token {
	return n.token
}

func (n *UnaryNode) showtree(depth string) {
	fmt.Println(depth + "- " + n.print())
}

func NewUnaryNode(tok *Token, nodetype NodeType) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		nodetype: nodetype,
	}
}
