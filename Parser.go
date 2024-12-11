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
	tokens              []Token
	current             Token
	current_token_index int
	errors              []*Token
	errorCheck          bool
}

func NewParser(tokens []Token) *Parser {
	return &Parser{
		tokens:              tokens,
		current:             tokens[0],
		current_token_index: 0,
		errors:              nil,
		errorCheck:          false,
	}
}

func (p *Parser) parse_program() ParseNode {
	root := p.statements()

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.lexeme)
		}
		p.errorCheck = true
	}

	return root
}

func (p *Parser) statements() ParseNode {
	list_node := ModuleRootNode()

	for {
		// consume all newlines until we reach actual code
		for p.match_types(p.current.token_type, NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.token_type, EOF) {
			break
		}

		first_statement := p.expression()
		list_node.children = append(list_node.children, first_statement)
	}

	return list_node
}

func (p *Parser) expression() ParseNode {
	return p.parse_operator(p.parse_primary(), EQUALITY)
}

func (p *Parser) parse_operator(left ParseNode, min_precedence TokenType) ParseNode {
	current := p.current
	for is_operator(current.token_type) && current.token_type >= min_precedence {
		op := current
		p.advance()

		right := p.parse_primary()
		current = p.current
		for is_operator(current.token_type) && current.token_type > op.token_type ||
			(p.op_is_right_associative(current.lexeme) && current.token_type == op.token_type) {

			inc := decide_precedence_inc(current.token_type, op.token_type)
			right = p.parse_operator(right, TokenType(int(op.token_type)+inc))

			current = p.current
		}
		left = NewBinaryNode(&op, EXPRESSION, left, right)
	}

	return left
}

func (p *Parser) parse_primary() ParseNode {
	if p.match_types(p.current.token_type, NUM) {
		ret := NewUnaryNode(&p.current, NUMBER)
		p.advance() // get the number that was matched and update current with the next token in the queue
		return ret
	}

	if p.match_types(p.current.token_type, LPAREN) {
		p.consume(LPAREN, "Expected closing ')'")
		expr := p.expression()
		p.consume(RPAREN, "Expected closing ')'")

		return expr
	}

	return nil
}

func is_operator(tokentype TokenType) bool {
	// NOTE: 3 is the highest operator precedence enum value, everything after 3 is other token types
	return tokentype <= 3
}

func decide_precedence_inc(curr_op_type TokenType, op_type TokenType) int {
	if curr_op_type > op_type {
		return 1
	}
	return 0
}

//// PARSER UTILITIES ////

func (p *Parser) match_types(type_a TokenType, type_b TokenType) bool {
	return type_a == type_b
}

// if the current token is token_type then advance otherwise throw an error
// used to expected certain tokens in the input
func (p *Parser) consume(token_type TokenType, message string) Token {
	if p.current_token_type_is(token_type) {
		p.advance()
		return p.current
	}

	error := ErrorToken(message, p.current.linenum)
	p.errors = append(p.errors, error)
	return *error
}

func (p *Parser) advance() {
	// if the current token index is already at the end
	//   then cap the index at the end of the array
	if p.current_token_index == len(p.tokens)-1 {
		p.current_token_index = len(p.tokens) - 1
		p.current = p.tokens[p.current_token_index]
		return
	}

	p.current_token_index = p.current_token_index + 1
	p.current = p.tokens[p.current_token_index]
}

func (p *Parser) peek(amount int) Token {
	if p.current_token_index+amount >= len(p.tokens)-1 {
		return p.tokens[len(p.tokens)-1]
	}

	return p.tokens[p.current_token_index+amount]
}

// checks the current token against the type parameter
func (p *Parser) current_token_type_is(token_type TokenType) bool {
	if p.is_ending_token() {
		return false
	}

	return p.current.token_type == token_type
}

func (p *Parser) is_ending_token() bool {
	return p.current.token_type == EOF
}

func (p *Parser) op_is_left_associative(op_literal string) bool {
	var left_map map[string]bool = map[string]bool{
		"*":  true,
		"/":  true,
		"-":  true,
		"+":  true,
		"==": true,
		"^":  false,
	}
	return left_map[op_literal]
}

func (p *Parser) op_is_right_associative(op_literal string) bool {
	return !p.op_is_left_associative(op_literal)
}
