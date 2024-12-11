package main

import (
	"fmt"
)

// OPERATOR ENUMS
type TokenType int

const (
	EQUALITY TokenType = 0
	ADD      TokenType = 1
	SUB      TokenType = 1
	MUL      TokenType = 2
	DIV      TokenType = 2
	EXP      TokenType = 3
)

// TOKEN ENUMS
const (
	NUM TokenType = iota + 4
	IDENT
	KEYWORDTOK
	NEWLINE
	ASSIGN
	LPAREN
	RPAREN
	STRING
	EOF
	SPACE
	ERROR
)

type Token struct {
	token_type TokenType
	lexeme     string
	linenum    int
}

func ErrorToken(message string, ln int) *Token {
	return &Token{
		token_type: ERROR,
		lexeme:     message,
		linenum:    ln,
	}
}

func PrintTokenType(token_type TokenType) string {
	typeMap := map[TokenType]string{
		NUM: "NUMBER", IDENT: "IDENTIFIER",
		KEYWORDTOK: "KEYWORD", NEWLINE: "NEWLINE",
		ASSIGN: "ASSIGN", LPAREN: "LPAREN",
		RPAREN: "RPAREN", EOF: "EOF",
		SPACE: "SPACE", ERROR: "ERROR",
	}

	mapping, err := typeMap[token_type]
	if !err {
		panic(fmt.Sprintf("Non existent type provided to token type: %d\n", token_type))
	}
	return mapping
}

func (t *Token) show() string {
	return fmt.Sprintf("Token{%s, %s, %d}", PrintTokenType(t.token_type), t.lexeme, t.linenum)
}

func (t *Token) print() {
	fmt.Println(t.show())
}
