package main

import (
	"fmt"
)

const (
	NUMBER = iota
	IDENTIFIER
	KEYWORD
	NEWLINE
	MUL
	DIV
	ADD
	SUB
	ASSIGN
	LPAREN
	RPAREN
	ROOT // this is for the root module nodes at the beginning of the parse tree
	EOF
	SPACE
	ERROR
)

type Token struct {
	ttype   int
	lexeme  string
	linenum int
}

func ErrorToken(message string, ln int) *Token {
	return &Token{
		ttype:   ERROR,
		lexeme:  message,
		linenum: ln,
	}
}

func PrintTokenType(ttype int) string {
	typeMap := map[int]string{
		NUMBER: "NUMBER", IDENTIFIER: "IDENTIFIER",
		KEYWORD: "KEYWORD", NEWLINE: "NEWLINE",
		MUL: "MUL", DIV: "DIV",
		ADD: "ADD", SUB: "SUB",
		ASSIGN: "ASSIGN", LPAREN: "LPAREN",
		RPAREN: "RPAREN",
	}

	mapping, err := typeMap[ttype]
	if !err {
		panic("Non existent type provided to token type")
	}
	return mapping
}

func (t *Token) show() string {
	return fmt.Sprintf("Token{%s, %s, %d}", PrintTokenType(t.ttype), t.lexeme, t.linenum)
}

func (t *Token) print() {
	fmt.Println(t.show())
}
