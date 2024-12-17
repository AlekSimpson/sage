package main

import (
	"unicode"
)

type Lexer struct {
	buffer       *Queue[byte]
	tokens       *Queue[Token]
	linenum      int
	current_char rune
	last_token   Token
}

func NewLexer(buffer *Queue[byte]) Lexer {
	return Lexer{
		buffer:  buffer,
		tokens:  NewEmptyQueue(NewErrorToken("empty queue", -1)),
		linenum: 0,
	}
}

func (l *Lexer) check_for_string() *Token {
	lexeme := ""
	if l.current_char == '"' {
		// add the first '"' character to kick off the loop
		lexeme += "\""
		l.current_char = rune(l.buffer.pop())

		for l.current_char != '"' {
			lexeme += string(l.current_char)
			l.current_char = rune(l.buffer.pop())
		}

		lexeme += "\""
		return &Token{TT_STRING, lexeme, l.linenum}
	}

	return nil
}

func (l *Lexer) lex_for_symbols() *Token {
	// multi-char symbols: ::, ->, ..., ==, -- ++, >=, <=, &&, ||
	return_val := l.check_for_string()
	if return_val != nil {
		// if the return isn't nil that means it did find a string
		return return_val
	}

	var SYMBOLS = map[rune]TokenType{
		'(': TT_LPAREN,
		')': TT_RPAREN,
		'*': TT_MUL,
		'/': TT_DIV,
		',': TT_COMMA,
		'[': TT_LBRACKET,
		']': TT_RBRACKET,
		'{': TT_LBRACE,
		'}': TT_RBRACE,
	}

	switch l.current_char {
	case ':':
		return l.followed_by(':', TT_BINDING, "::")

	case '-':
		peekahead := rune(l.buffer.pop())
		if peekahead == '>' {
			return &Token{TT_FUNC_RETURN_TYPE, "->", l.linenum}
		} else if peekahead == '-' {
			return &Token{TT_DECREMENT, "--", l.linenum}
		}
		return &Token{TT_SUB, "-", l.linenum}

	case '.':
		first_peek := rune(l.buffer.pop())
		l.current_char = first_peek

		if l.last_token.token_type == TT_IDENT && first_peek != '.' {
			return &Token{TT_FIELD_ACCESSOR, ".", l.linenum}
		}

		if first_peek == '.' {
			second_peek := rune(l.buffer.pop())
			if second_peek == '.' {
				return &Token{TT_RANGE, "...", l.linenum}
			}
			return nil
		}

		return nil

	case '=':
		ret := l.followed_by('=', TT_EQUALITY, "==")
		if ret != nil {
			return ret
		}
		return &Token{TT_ASSIGN, "=", l.linenum}

	case '+':
		ret := l.followed_by('+', TT_INCREMENT, "++")
		if ret != nil {
			return ret
		}
		return &Token{TT_ADD, "+", l.linenum}

	case '>':
		ret := l.followed_by('=', TT_GTE, ">=")
		if ret != nil {
			return ret
		}
		return &Token{TT_GT, ">", l.linenum}

	case '<':
		ret := l.followed_by('=', TT_LTE, "<=")
		if ret != nil {
			return ret
		}
		return &Token{TT_LT, "<", l.linenum}

	case '&':
		ret := l.followed_by('&', TT_AND, "&&")
		if ret != nil {
			return ret
		}
		return &Token{TT_BIT_AND, "&", l.linenum}

	case '|':
		ret := l.followed_by('|', TT_OR, "||")
		if ret != nil {
			return ret
		}
		return &Token{TT_BIT_OR, "|", l.linenum}

	default:
		tok_type, exists := SYMBOLS[l.current_char]
		if !exists {
			return nil
		}
		return &Token{tok_type, string(l.current_char), l.linenum}
	}
}

func (l *Lexer) followed_by(expected_char rune, expected_type TokenType, expected_lexeme string) *Token {
	l.current_char = rune(l.buffer.pop())
	if l.current_char == expected_char {
		return &Token{expected_type, expected_lexeme, l.linenum}
	}
	return nil
}

func (l *Lexer) lex_for_numbers() *Token {
	if !unicode.IsDigit(l.current_char) {
		return nil
	}

	is_a_float := false
	dot_count := 0
	lexeme := ""
	for unicode.IsDigit(l.current_char) || (l.current_char == '.' && dot_count == 0) {
		if l.current_char == '.' {
			is_a_float = true
			dot_count++
		}

		lexeme += string(l.current_char)
		l.current_char = rune(l.buffer.pop())
	}

	// if the number ends in a '.' then this is a number before the range operator and not actually a float number
	if lexeme[len(lexeme)-1] == '.' {
		// in which case we want to set it back to a regular number, remove the last period character and put it back in the char buffer
		lexeme = lexeme[:len(lexeme)-1]
		l.buffer.stack(byte('.'))
		is_a_float = false
	}

	// Add the last read char back onto to the top of the buffer because it was not matched to the current Token
	l.buffer.stack(byte(l.current_char))

	token := &Token{TT_NUM, lexeme, l.linenum}
	if is_a_float {
		token.token_type = TT_FLOAT
	}

	return token
}

func (l *Lexer) lex_for_identifiers() *Token {
	if !unicode.IsLetter(l.current_char) && l.current_char != '_' {
		return nil
	}

	lexeme := ""

	KEYWORDS := map[string]string{
		"int":         "INT",
		"char":        "CHAR",
		"void":        "VOID",
		"include":     "INCLUDE",
		"for":         "FOR",
		"while":       "WHILE",
		"in":          "IN",
		"if":          "IF",
		"else":        "ELSE",
		"break":       "BREAK",
		"continue":    "CONTINUE",
		"fallthrough": "FALLTHROUGH", // like the break keyword but for nested loops, if used inside a nested loop it will break out of all loops
		"ret":         "RETURN",
		"struct":      "STRUCT",
	}

	for unicode.IsLetter(l.current_char) || l.current_char == '_' {
		lexeme += string(l.current_char)
		l.current_char = rune(l.buffer.pop())
	}

	// Add the last read char back onto to the top of the buffer because it was not matched to the current Token
	l.buffer.stack(byte(l.current_char))

	token := Token{TT_IDENT, lexeme, l.linenum}
	_, err := KEYWORDS[lexeme]
	if err {
		// check if the word is a keyword
		token.token_type = TT_KEYWORD
	}

	return &token
}

func (l *Lexer) get_token() Token {
	if !l.tokens.empty() {
		tok := l.tokens.pop()
		return tok
	}

	if l.buffer.empty() {
		l.last_token = Token{TT_EOF, "eof", l.linenum + 1}
		return l.last_token
	}

	l.current_char = rune(l.buffer.pop())

	for l.current_char == ' ' || l.current_char == '\t' {
		l.current_char = rune(l.buffer.pop())
	}

	if l.current_char == '\n' {
		token := Token{TT_NEWLINE, "newline", l.linenum}
		l.linenum += 1
		l.last_token = token
		return token
	}

	token := l.lex_for_symbols()
	if token != nil {
		l.last_token = *token
		return *token
	}

	token = l.lex_for_identifiers()
	if token != nil {
		l.last_token = *token
		return *token
	}

	token = l.lex_for_numbers()
	if token != nil {
		l.last_token = *token
		return *token
	}

	l.last_token = Token{TT_ERROR, "unrecognized symbol", l.linenum + 1}
	return l.last_token
}

func (l *Lexer) unget_token() {
	l.tokens.stack(l.last_token)
}
