package sage

import (
	"fmt"
)

// OPERATOR ENUMS
type TokenType int

const (
	TT_EQUALITY TokenType = 0
	TT_LT       TokenType = 1
	TT_GT       TokenType = 1
	TT_GTE      TokenType = 2
	TT_LTE      TokenType = 2
	TT_ADD      TokenType = 3
	TT_SUB      TokenType = 3
	TT_MUL      TokenType = 4
	TT_DIV      TokenType = 4
	TT_EXP      TokenType = 5

	TT_NUM TokenType = iota + 6
	TT_IDENT
	TT_FLOAT
	TT_KEYWORD
	TT_NEWLINE
	TT_ASSIGN
	TT_LPAREN
	TT_RPAREN
	TT_LBRACE
	TT_RBRACE
	TT_LBRACKET
	TT_RBRACKET
	TT_COMMA
	TT_FUNC_RETURN_TYPE
	TT_INCLUDE
	TT_STRING
	TT_EOF
	TT_SPACE
	TT_STAR
	TT_ERROR
	TT_BINDING
	TT_RANGE
	TT_COMPILER_CREATED
	TT_BIT_AND
	TT_BIT_OR
	TT_DECREMENT
	TT_INCREMENT
	TT_AND
	TT_OR
	TT_FIELD_ACCESSOR // 'struct_name.value'
	TT_POUND
)

type Token struct {
	Token_type TokenType
	Lexeme     string
	Linenum    int
}

func NewToken(token_type TokenType, lexeme string, linenum int) Token {
	return Token{
		Token_type: token_type,
		Lexeme:     lexeme,
		Linenum:    linenum,
	}
}

func NewErrorToken(message string, ln int) Token {
	return Token{
		Token_type: TT_ERROR,
		Lexeme:     message,
		Linenum:    ln,
	}
}

func (tt TokenType) String() string {
	switch tt {
	case TT_NUM:
		return "TT_NUM"
	case TT_IDENT:
		return "TT_IDENT"
	case TT_FLOAT:
		return "TT_FLOAT"
	case TT_KEYWORD:
		return "TT_KEYWORD"
	case TT_NEWLINE:
		return "TT_NEWLINE"
	case TT_ASSIGN:
		return "TT_ASSIGN"
	case TT_LPAREN:
		return "TT_LPAREN"
	case TT_RPAREN:
		return "TT_RPAREN"
	case TT_LBRACE:
		return "TT_LBRACE"
	case TT_RBRACE:
		return "TT_RBRACE"
	case TT_LBRACKET:
		return "TT_LBRACKET"
	case TT_RBRACKET:
		return "TT_RBRACKET"
	case TT_COMMA:
		return "TT_COMMA"
	case TT_FUNC_RETURN_TYPE:
		return "TT_FUNC_RETURN_TYPE"
	case TT_INCLUDE:
		return "TT_INCLUDE"
	case TT_STRING:
		return "TT_STRING"
	case TT_EOF:
		return "TT_EOF"
	case TT_SPACE:
		return "TT_SPACE"
	case TT_STAR:
		return "TT_STAR"
	case TT_ERROR:
		return "TT_ERROR"
	case TT_BINDING:
		return "TT_BINDING"
	case TT_RANGE:
		return "TT_RANGE"
	case TT_COMPILER_CREATED:
		return "TT_COMPILER_CREATED"
	case TT_BIT_AND:
		return "TT_BIT_AND"
	case TT_BIT_OR:
		return "TT_BIT_OR"
	case TT_DECREMENT:
		return "TT_DECREMENT"
	case TT_INCREMENT:
		return "TT_INCREMENT"
	case TT_GTE:
		return "TT_GTE or TT_LTE"
	case TT_AND:
		return "TT_AND"
	case TT_OR:
		return "TT_OR"
	case TT_LT:
		return "TT_LT or TT_GT"
	case TT_FIELD_ACCESSOR:
		return "TT_FIELD_ACCESSOR"
	case TT_EQUALITY:
		return "TT_EQUALITY"
	case TT_ADD:
		return "TT_ADD or TT_SUB"
	case TT_MUL:
		return "TT_MUL or TT_DIV"
	case TT_EXP:
		return "TT_EXP"
	case TT_POUND:
		return "TT_POUND"
	default:
		return "Unkown Token Type"
	}
}

func (t *Token) Show() string {
	return fmt.Sprintf("Token{%s, %s, %d}", t.Token_type.String(), t.Lexeme, t.Linenum)
}

func (t *Token) Print() {
	fmt.Println(t.Show())
}
