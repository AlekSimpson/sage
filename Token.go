package main

import (
	"fmt"
)

// OPERATOR ENUMS
type TokenType int

const (
	TT_EQUALITY TokenType = 0
	TT_ADD      TokenType = 1
	TT_SUB      TokenType = 1
	TT_MUL      TokenType = 2
	TT_DIV      TokenType = 2
	TT_EXP      TokenType = 3
)

// TOKEN ENUMS
const (
	TT_NUM TokenType = iota + 4
	TT_IDENT
	TT_KEYWORDTOK
	TT_NEWLINE
	TT_ASSIGN
	TT_LPAREN
	TT_RPAREN
	TT_STRING
	TT_EOF
	TT_SPACE
	TT_ERROR
	TT_INCLUDE
	TT_RETURN
	TT_FOR
	TT_WHILE
	TT_BINDING
	TT_COMPILER_CREATED
)

type Token struct {
	token_type TokenType
	lexeme     string
	linenum    int
}

func ErrorToken(message string, ln int) *Token {
	return &Token{
		token_type: TT_ERROR,
		lexeme:     message,
		linenum:    ln,
	}
}

func PrintTokenType(token_type TokenType) string {
	typeMap := map[TokenType]string{
		TT_NUM: "NUMBER", TT_IDENT: "IDENTIFIER",
		TT_KEYWORDTOK: "KEYWORD", TT_NEWLINE: "NEWLINE",
		TT_ASSIGN: "ASSIGN", TT_LPAREN: "LPAREN",
		TT_RPAREN: "RPAREN", TT_EOF: "EOF",
		TT_SPACE: "SPACE", TT_ERROR: "ERROR",
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
