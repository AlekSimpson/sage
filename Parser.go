package main

import (
	"fmt"
)

// EXPRESSION
// COMPARISON
// UNARY
// MULT/DIV
// ADD/SUB
// NUMBER/ID/EXPRESSION

// expression := equality
// equality   := comparison ("!=" | "==") comparison
// comparison := term (">" | "<" | ">=" | "<=") term
// term       := factor ("-" | "+") factor
// factor     := primary ("*" | "/") primary
// unary
// primary    := NUMBER | "(" expression ")"

type Parser struct {
	tokens     *Queue[Token]
	current    Token
	errors     []*Token
	errorCheck bool
}

func NewParser(tokens []Token) *Parser {
	token_buffer := NewQueue(tokens, Token{})

	return &Parser{
		tokens:     token_buffer,
		current:    token_buffer.pop(),
		errors:     nil,
		errorCheck: false,
	}
}

func (p *Parser) parse() ParseNode {
	root := ModuleRootNode()

	for !p.isAtEnd() {
		// continue parsing until all lines in the source file are parsed
		root.children = append(root.children, p.expression())

		if p.current.ttype == NEWLINE {
			p.consume(NEWLINE, "expected new line at end of expression parsing")
		} else {
			break
		}
	}

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.lexeme)
		}
		p.errorCheck = true
	}

	return root
}

func (p *Parser) block() ParseNode {
	return nil
}

func (p *Parser) expression() ParseNode {
	return p.equality()
}

func (p *Parser) equality() ParseNode {
	return p.comparison()
}

func (p *Parser) comparison() ParseNode {
	return p.term()
}

func (p *Parser) term() ParseNode {
	left := p.factor()

	if p.match([]int{ADD, SUB}) {
		operator := p.advance()
		right := p.term()

		return NewBinaryNode(&operator, left, right)
	}

	return left
}

func (p *Parser) factor() ParseNode {
	left := p.primary()

	if p.match([]int{MUL, DIV}) {
		operator := p.advance()
		right := p.factor()

		return NewBinaryNode(&operator, left, right)
	}

	return left
}

func (p *Parser) primary() ParseNode {
	if p.match([]int{NUMBER}) {
		num := p.advance() // get the number that was matched and update current with the next token in the queue
		return NewUnaryNode(&num)
	}

	if p.match([]int{LPAREN}) {
		p.advance() // advance past the '(' token
		expr := p.expression()
		p.consume(RPAREN, "Expected closing ')'")

		return expr
	}

	var error = ErrorToken(fmt.Sprintf("Error found on line %d\n", p.current.linenum), p.current.linenum)
	p.errors = append(p.errors, error)

	return nil
}

//// PARSER UTILITIES ////

func (p *Parser) match(types []int) bool {
	for _, t := range types {
		if p.check(t) {
			return true
		}
	}
	return false
}

func (p *Parser) consume(t int, message string) Token {
	if p.check(t) {
		return p.advance()
	}

	error := ErrorToken(message, p.current.linenum)
	p.errors = append(p.errors, error)
	return *error
}

func (p *Parser) advance() Token {
	// returns the old "current" token and updates current with the next token in the queue
	oldtok := p.current
	p.current = p.tokens.pop()
	return oldtok
}

// checks the current token against the type parameter
func (p *Parser) check(t int) bool {
	if p.isAtEnd() {
		return false
	}

	return p.current.ttype == t
}

func (p *Parser) isAtEnd() bool {
	return p.current.ttype == EOF
}
