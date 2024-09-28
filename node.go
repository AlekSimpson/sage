package main

import (
	"fmt"
)

type Printable func() string

type ParseNode struct {
	token    *Token
	left     *ParseNode
	right    *ParseNode
	children []*ParseNode
	print    Printable
}

//// BEGIN BINARY NODE ////

func PrintBinaryNode(tok *Token, left *ParseNode, right *ParseNode) string {
	return fmt.Sprintf("BinaryNode{%s, %s, %s}", tok.show(), left.token.show(), right.token.show())
}

func BinaryNode(t *Token, l *ParseNode, r *ParseNode) *ParseNode {
	return &ParseNode{
		token: t,
		left:  l,
		right: r,
		print: func() string { return PrintBinaryNode(t, l, r) },
	}
}

//// BEGIN BLOCK NODE ////

func PrintBlockNode(tok *Token, children []*ParseNode) string {
	var retval string
	retval += fmt.Sprintf("BlockNode{%s, ", tok.show())
	for _, child := range children {
		retval += fmt.Sprintf("%s, ", child.print())
	}
	retval += fmt.Sprintf("%s}\n")
	return retval
}

func BlockNode(tok *Token, c []*ParseNode) *ParseNode {
	return &ParseNode{
		token:    tok,
		children: c,
		print:    func() string { return PrintBlockNode(tok, c) },
	}
}

//// BEGIN MODULE ROOT NODE ////

func ModuleRootNode() *ParseNode {
	rootTok := &Token{
		ttype:   ROOT,
		lexeme:  "",
		linenum: 0,
	}

	children := make([]*ParseNode, 0)

	return &ParseNode{
		token:    rootTok,
		children: children,
		print:    func() string { return PrintBlockNode(rootTok, children) },
	}
}

//// BEGIN UNARY NODE ////

func PrintUnaryNode(tok *Token) string {
	var retval string
	retval += fmt.Sprintf("UnaryNode{%s}\n", tok.show())
	return retval
}

func UnaryNode(tok *Token) *ParseNode {
	return &ParseNode{
		token: tok,
		print: func() string { return PrintUnaryNode(tok) },
	}
}
