package main

import (
	"fmt"
)

type NodeType int

const (
	NUMBER NodeType = iota
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
	BINARY
	PROGRAM
	EXPRESSION
)

type ParseNode interface {
	print() string
	showtree(depth string)
	get_token() *Token
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
	return fmt.Sprintf("BINARY NODE (%s %s %s)", n.token.lexeme, n.left.get_token().lexeme, n.right.get_token().lexeme)
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
