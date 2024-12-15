package main

import (
	"unicode"
)

type Lexer struct {
	buffer  *Queue[byte]
	tokens  []Token
	linenum int
	char    rune
}

func (l *Lexer) lexForSymbols() {
	var SYMBOLS = map[rune]TokenType{
		'(': TT_LPAREN,
		')': TT_RPAREN,
		'+': TT_ADD,
		'-': TT_SUB,
		'*': TT_MUL,
		'/': TT_DIV,
		'=': TT_ASSIGN,
	}

	sym, err := SYMBOLS[l.char]
	if !err {
		return
	}

	token := Token{sym, string(l.char), l.linenum}
	l.char = rune(l.buffer.pop())
	l.tokens = append(l.tokens, token)
}

func (l *Lexer) lexForNumber() {
	if !unicode.IsDigit(l.char) {
		return
	}

	lexeme := ""
	for unicode.IsDigit(l.char) {
		lexeme += string(l.char)
		l.char = rune(l.buffer.pop())
	}

	token := Token{TT_NUM, lexeme, l.linenum}
	l.tokens = append(l.tokens, token)
}

func (l *Lexer) lexForIdentifier() {
	if !unicode.IsLetter(l.char) && l.char != '_' {
		return
	}

	lexeme := ""

	KEYWORDS := map[string]string{
		"define": "DEFINE",
		"let":    "LET",
	}

	for unicode.IsLetter(l.char) || l.char == '_' {
		lexeme += string(l.char)
		l.char = rune(l.buffer.pop())
	}

	token := Token{TT_IDENT, lexeme, l.linenum}
	_, err := KEYWORDS[lexeme]
	if err {
		// check if the word is a keyword
		token.token_type = TT_KEYWORDTOK
	}

	l.tokens = append(l.tokens, token)
}

func (l *Lexer) lex() []Token {
	l.char = rune(l.buffer.pop())

	for !l.buffer.empty() {
		if l.char == ' ' || l.char == '\t' {
			l.char = rune(l.buffer.pop())
			continue
		}

		if l.char == '\n' {
			token := Token{TT_NEWLINE, string(l.char), l.linenum}
			l.linenum += 1
			l.tokens = append(l.tokens, token)
			l.char = rune(l.buffer.pop())
		}

		l.lexForSymbols()
		l.lexForIdentifier()
		l.lexForNumber()
	}

	l.tokens = append(l.tokens, Token{TT_EOF, "eof", l.linenum + 1})

	return l.tokens
}
