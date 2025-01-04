package sage

import (
	"unicode"
)

type Lexer struct {
	buffer       *Queue[byte]
	tokens       *Queue[Token]
	linenum      int
	linedepth    int
	current_char rune
	last_token   Token
	filename     string
}

func (l *Lexer) LexerMakeToken(token_type TokenType, lexeme string) *Token {
	return &Token{
		Token_type: token_type,
		Lexeme:     lexeme,
		Linenum:    l.linenum,
		Linedepth:  l.linedepth,
		Filename:   l.filename,
	}
}

func NewLexer(filename string, buffer *Queue[byte]) Lexer {
	return Lexer{
		buffer:    buffer,
		tokens:    NewEmptyQueue[Token](),
		linenum:   0,
		linedepth: 0,
		filename:  filename,
	}
}

func (l *Lexer) check_for_string() *Token {
	lexeme := ""
	if l.current_char == '"' {
		// add the first '"' character to kick off the loop
		lexeme += "\""
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++

		for l.current_char != '"' {
			lexeme += string(l.current_char)
			l.current_char = rune(*l.buffer.Pop())
			l.linedepth++
		}

		lexeme += "\""
		return l.LexerMakeToken(TT_STRING, lexeme)
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
		'#': TT_POUND,
	}

	switch l.current_char {
	case ':':
		return l.followed_by(':', TT_BINDING, "::")

	case '-':
		peekahead := rune(*l.buffer.Pop())
		if peekahead == '>' {
			return l.LexerMakeToken(TT_FUNC_RETURN_TYPE, "->")
		} else if peekahead == '-' {
			return l.LexerMakeToken(TT_DECREMENT, "--")
		}
		return l.LexerMakeToken(TT_SUB, "-")

	case '.':
		first_peek := rune(*l.buffer.Pop())
		l.current_char = first_peek

		// if the '.' is in between an identifier token and unicode chars then its a field accessor
		if l.last_token.Token_type == TT_IDENT && (unicode.IsLetter(first_peek) || first_peek == '_') {
			l.buffer.Stack(byte(first_peek))
			return l.LexerMakeToken(TT_FIELD_ACCESSOR, ".")
		}

		if first_peek == '.' {
			second_peek := rune(*l.buffer.Pop())
			if second_peek == '.' {
				return l.LexerMakeToken(TT_RANGE, "...")
			}
			return nil
		}

		return nil

	case '=':
		ret := l.followed_by('=', TT_EQUALITY, "==")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_ASSIGN, "=")

	case '+':
		ret := l.followed_by('+', TT_INCREMENT, "++")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_ADD, "+")

	case '>':
		ret := l.followed_by('=', TT_GTE, ">=")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_GT, ">")

	case '<':
		ret := l.followed_by('=', TT_LTE, "<=")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_LT, "<")

	case '&':
		ret := l.followed_by('&', TT_AND, "&&")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_BIT_AND, "&")

	case '|':
		ret := l.followed_by('|', TT_OR, "||")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(TT_BIT_OR, "|")

	default:
		tok_type, exists := SYMBOLS[l.current_char]
		if !exists {
			return nil
		}
		return l.LexerMakeToken(tok_type, string(l.current_char))
	}
}

func (l *Lexer) followed_by(expected_char rune, expected_type TokenType, expected_lexeme string) *Token {
	l.current_char = rune(*l.buffer.Pop())
	l.linedepth++
	if l.current_char == expected_char {
		return l.LexerMakeToken(expected_type, expected_lexeme)
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
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	// if the number ends in a '.' then this is a number before the range operator and not actually a float number
	if lexeme[len(lexeme)-1] == '.' {
		// in which case we want to set it back to a regular number, remove the last period character and put it back in the char buffer
		lexeme = lexeme[:len(lexeme)-1]
		l.buffer.Stack(byte('.'))
		is_a_float = false
	}

	// Add the last read char back onto to the top of the buffer because it was not matched to the current Token
	l.buffer.Stack(byte(l.current_char))

	token := l.LexerMakeToken(TT_NUM, lexeme)
	if is_a_float {
		token.Token_type = TT_FLOAT
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
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	// Add the last read char back onto to the top of the buffer because it was not matched to the current Token
	l.buffer.Stack(byte(l.current_char))

	token := l.LexerMakeToken(TT_IDENT, lexeme)
	_, err := KEYWORDS[lexeme]
	if err {
		// check if the word is a keyword
		token.Token_type = TT_KEYWORD
	}

	return token
}

func (l *Lexer) Get_token() Token {
	if !l.tokens.empty() {
		tok := l.tokens.Pop()
		return *tok
	}

	if l.buffer.empty() {
		l.last_token = *l.LexerMakeToken(TT_EOF, "eof")
		return l.last_token
	}

	l.current_char = rune(*l.buffer.Pop())
	l.linedepth++

	for l.current_char == ' ' || l.current_char == '\t' {
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	if l.current_char == '\n' {
		token := *l.LexerMakeToken(TT_NEWLINE, "newline")
		l.linenum += 1
		l.linedepth = 0
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

	l.last_token = *l.LexerMakeToken(TT_ERROR, "unrecognized symbol")
	return l.last_token
}

func (l *Lexer) Unget_token() {
	l.tokens.Stack(l.last_token)
}
