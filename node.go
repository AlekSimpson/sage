package main

import (
	"fmt"
)

type ParseNode interface {
	print() string
	showtree(depth int)
}

//// BEGIN BLOCK NODE

type BlockNode struct {
	token    *Token
	children []ParseNode
}

func ModuleRootNode() *BlockNode {
	rootTok := &Token{
		ttype:   ROOT,
		lexeme:  "",
		linenum: 0,
	}

	return &BlockNode{
		token: rootTok,
	}
}

func NewBlockNode(tok *Token, c []ParseNode) *BlockNode {
	return &BlockNode{
		token:    tok,
		children: c,
	}
}

func (n *BlockNode) formatChildren() string {
	if len(n.children) == 0 {
		return "NO CHILDREN"
	}

	retval := ""
	for _, child := range n.children {
		retval += (child.print() + ", ")
	}
	retval = retval[:len(retval)-2]
	return retval
}

func (n *BlockNode) print() string {
	return fmt.Sprintf("BlockNode{ token: %s,  children: (\n\t%s\n) }", n.token.show(), n.formatChildren())
}

//	func (tn *TreeNode) print(indent string) {
//		fmt.Println(indent + "- " + tn.node.value)
//
//		if tn.left != nil {
//			tn.left.print(indent + "\t")
//		}
//		if tn.right != nil {
//			tn.right.print(indent + "\t")
//		}
//	}
func (n *BlockNode) showtree(depth int) {
}

//// BEGIN BINARY NODE

type BinaryNode struct {
	token *Token
	left  ParseNode
	right ParseNode
}

func NewBinaryNode(t *Token, l ParseNode, r ParseNode) *BinaryNode {
	return &BinaryNode{
		token: t,
		left:  l,
		right: r,
	}
}

func (n *BinaryNode) print() string {
	return fmt.Sprintf("BinaryNode{ token: %s, left: %s,  right: %s }", n.token.show(), n.left.print(), n.right.print())
}

//// BEGIN UNARY NODE

type UnaryNode struct {
	token *Token
}

func (n *UnaryNode) print() string {
	var retval string
	retval += fmt.Sprintf("UnaryNode{%s}", n.token.show())
	return retval
}

func NewUnaryNode(tok *Token) *UnaryNode {
	return &UnaryNode{
		token: tok,
	}
}
