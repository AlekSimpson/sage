package main

import (
    "fmt"
)

type Printable func()

type ParseNode struct {
    token     *Token
    left      *ParseNode
    right     *ParseNode
    children  []*ParseNode
    print     Printable
}

func PrintBinaryNode(tok *Token, left *ParseNode, right *ParseNode) {
    fmt.Println(fmt.Sprintf("BinaryNode{%s, %s, %s}", tok.show(), left.token.show(), right.token.show()))
}

func BinaryNode(t *Token, l *ParseNode, r *ParseNode) *ParseNode {
    return &ParseNode{
        token: t,
        left: l,
        right: r,
        print: func() { PrintBinaryNode(t, l, r) },
    }
}


func PrintBlockNode(tok *Token, childen []*ParseNode) {
    fmt.Print("BlockNode{", tok.show())

}

func BlockNode(c []*ParseNode) *ParseNode {
    return &ParseNode{
        children: c,
    }
}


