package sage

import (
	q "sage/internal/queues"
	t "sage/internal/tokens"
	"unicode"
)

type Lexer struct {
	buffer       *q.Queue[byte]
	tokens       *q.Queue[t.Token]
	linenum      int
	linedepth    int
	current_char rune
	last_token   t.Token
	filename     string
}

func (l *Lexer) LexerMakeToken(token_type t.TokenType, lexeme string) *t.Token {
	return &t.Token{
		Token_type: token_type,
		Lexeme:     lexeme,
		Linenum:    l.linenum,
		Linedepth:  l.linedepth,
		Filename:   l.filename,
	}
}

func NewLexer(filename string, buffer *q.Queue[byte]) Lexer {
	return Lexer{
		buffer:    buffer,
		tokens:    q.NewEmptyQueue[t.Token](),
		linenum:   0,
		linedepth: 0,
		filename:  filename,
	}
}

func (l *Lexer) check_for_string() *t.Token {
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
		return l.LexerMakeToken(t.TT_STRING, lexeme)
	}

	return nil
}

func (l *Lexer) lex_for_symbols() *t.Token {
	// multi-char symbols: ::, ->, ..., ==, -- ++, >=, <=, &&, ||
	return_val := l.check_for_string()
	if return_val != nil {
		// if the return isn't nil that means it did find a string
		return return_val
	}

	var SYMBOLS = map[rune]t.TokenType{
		'(': t.TT_LPAREN,
		')': t.TT_RPAREN,
		'*': t.TT_MUL,
		'/': t.TT_DIV,
		',': t.TT_COMMA,
		'[': t.TT_LBRACKET,
		']': t.TT_RBRACKET,
		'{': t.TT_LBRACE,
		'}': t.TT_RBRACE,
		'#': t.TT_POUND,
	}

	switch l.current_char {
	case ':':
		return l.followed_by(':', t.TT_BINDING, "::")

	case '-':
		peekahead := rune(*l.buffer.Pop())
		if peekahead == '>' {
			return l.LexerMakeToken(t.TT_FUNC_RETURN_TYPE, "->")
		} else if peekahead == '-' {
			return l.LexerMakeToken(t.TT_DECREMENT, "--")
		}
		return l.LexerMakeToken(t.TT_SUB, "-")

	case '.':
		first_peek := rune(*l.buffer.Pop())
		l.current_char = first_peek

		// if the '.' is in between an identifier token and unicode chars then its a field accessor
		if l.last_token.Token_type == t.TT_IDENT && (unicode.IsLetter(first_peek) || first_peek == '_') {
			l.buffer.Stack(byte(first_peek))
			return l.LexerMakeToken(t.TT_FIELD_ACCESSOR, ".")
		}

		if first_peek == '.' {
			second_peek := rune(*l.buffer.Pop())
			if second_peek == '.' {
				// if the last token that was found was a ',' and not a number then this is must be a vararg in a function signature
				tok_type := t.TT_RANGE
				if l.last_token.Token_type == t.TT_IDENT {
					tok_type = t.TT_VARARG
				}

				return l.LexerMakeToken(tok_type, "...")
			}
			return nil
		}

		return nil

	case '=':
		ret := l.followed_by('=', t.TT_EQUALITY, "==")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_ASSIGN, "=")

	case '+':
		ret := l.followed_by('+', t.TT_INCREMENT, "++")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_ADD, "+")

	case '>':
		ret := l.followed_by('=', t.TT_GTE, ">=")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_GT, ">")

	case '<':
		ret := l.followed_by('=', t.TT_LTE, "<=")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_LT, "<")

	case '&':
		ret := l.followed_by('&', t.TT_AND, "&&")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_BIT_AND, "&")

	case '|':
		ret := l.followed_by('|', t.TT_OR, "||")
		if ret != nil {
			return ret
		}
		return l.LexerMakeToken(t.TT_BIT_OR, "|")

	default:
		tok_type, exists := SYMBOLS[l.current_char]
		if !exists {
			return nil
		}
		return l.LexerMakeToken(tok_type, string(l.current_char))
	}
}

func (l *Lexer) followed_by(expected_char rune, expected_type t.TokenType, expected_lexeme string) *t.Token {
	l.current_char = rune(*l.buffer.Pop())
	l.linedepth++
	if l.current_char == expected_char {
		return l.LexerMakeToken(expected_type, expected_lexeme)
	}
	return nil
}

func (l *Lexer) lex_for_numbers() *t.Token {
	if !unicode.IsDigit(l.current_char) {
		return nil
	}

	dot_count := 0
	lexeme := ""
	tok_type := t.TT_NUM
	for unicode.IsDigit(l.current_char) || (l.current_char == '.' && dot_count == 0) {
		if l.current_char == '.' {
			tok_type = t.TT_FLOAT
			dot_count++
		}

		lexeme += string(l.current_char)
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	// if the number ends in a '.' then this is a number before the range operator and not actually a float number
	// we know this because the loop above handles floating point notation.
	if lexeme[len(lexeme)-1] == '.' {
		// in which case we want to set it back to a regular number, remove the last period character and put it back in the char buffer.
		tok_type = t.TT_NUM
		lexeme = lexeme[:len(lexeme)-1]
		// put the '.' back in the buffer because we want to properly part the symbol next
		l.buffer.Stack(byte('.'))
	}

	// add the last read char back onto to the top of the buffer because it was not matched to the current Token.
	l.buffer.Stack(byte(l.current_char))

	token := l.LexerMakeToken(tok_type, lexeme)

	return token
}

func (l *Lexer) lex_for_identifiers() *t.Token {
	if !unicode.IsLetter(l.current_char) && l.current_char != '_' {
		return nil
	}

	lexeme := ""

	KEYWORDS := map[string]string{
		"int":         "INT",
		"char":        "CHAR",
		"void":        "VOID",
		"i16":         "I16",
		"i32":         "I32",
		"i64":         "I64",
		"f32":         "F32",
		"f64":         "f64",
		"bool":        "BOOL",
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

	for unicode.IsLetter(l.current_char) || unicode.IsDigit(l.current_char) || l.current_char == '_' {
		lexeme += string(l.current_char)
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	// Add the last read char back onto to the top of the buffer because it was not matched to the current Token
	l.buffer.Stack(byte(l.current_char))

	token := l.LexerMakeToken(t.TT_IDENT, lexeme)
	_, err := KEYWORDS[lexeme]
	if err {
		// check if the word is a keyword
		token.Token_type = t.TT_KEYWORD
	}

	return token
}

func (l *Lexer) Get_token() t.Token {
	if !l.tokens.Empty() {
		tok := l.tokens.Pop()
		return *tok
	}

	if l.buffer.Empty() {
		l.last_token = *l.LexerMakeToken(t.TT_EOF, "eof")
		return l.last_token
	}

	l.current_char = rune(*l.buffer.Pop())
	l.linedepth++

	for l.current_char == ' ' || l.current_char == '\t' {
		l.current_char = rune(*l.buffer.Pop())
		l.linedepth++
	}

	if l.current_char == '\n' {
		token := *l.LexerMakeToken(t.TT_NEWLINE, "newline")
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

	l.last_token = *l.LexerMakeToken(t.TT_ERROR, "unrecognized symbol")
	return l.last_token
}

func (l *Lexer) Unget_token() {
	l.tokens.Stack(l.last_token)
}
