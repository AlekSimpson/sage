package main

import (
	"fmt"
)

// type Parser struct {
// 	tokens   *Queue[Token]
// 	curr_tok Token
// }
//
// func (p *Parser) NewParser(tokens []Token) *Parser {
// 	token_buffer := NewQueue(tokens)
//
// 	return &Parser{
// 		tokens:   token_buffer,
// 		curr_tok: token_buffer.pop(),
// 	}
// }
//
// func (p *Parser) generateParseTree() {
// }
//
// func (p *Parser) expression() {
// }
//
// func (p *Parser) comparison() {
// }
//
// func (p *Parser) unary() {
// }
//
// func (p *Parser) factor() {
// }
//
// func (p *Parser) term() {
// }
//
// func (p *Parser) atom() {
// }

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
	tokens  *Queue[Token]
	current Token
	errors  []*Token
}

func (p *Parser) NewParser(tokens []Token) *Parser {
	token_buffer := NewQueue(tokens)

	return &Parser{
		tokens:  token_buffer,
		current: token_buffer.pop(),
	}
}

func (p *Parser) parse() *ParseNode {
	p.cleanTokens()
	root := ModuleRootNode()

	for !p.isAtEnd() {
		// continue parsing until all lines in the source file are parsed

		root.children = append(root.children, p.expression())
		if p.peek().ttype == NEWLINE {
			p.consume(NEWLINE, "expected new line at end of expression parsing")
		} else {
			break
		}
	}

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.lexeme)
		}
		// root.errorCheck = true
	}

	return root
}

// func (p *Parser) block() *TreeNode {
// 	// TODO: this function is responsable for parsing any parts of the code that aggregates multiple expressions into one node, like loops, functions, types, etc.
// 	// if p.match([]int{IDENTIFIER})
//
// 	return nil
// }

func (p *Parser) expression() *ParseNode {
	return p.equality()
}

func (p *Parser) equality() *ParseNode {
	return p.comparison()
}

func (p *Parser) comparison() *ParseNode {
	return p.term()
}

func (p *Parser) term() *ParseNode {
	left := p.factor()

	if p.match([]int{ADD, SUB}) {
		operator := p.previous()
		right := p.term()
		return NewTreeNodeWith(operator, left, right)
	}

	return left
}

func (p *Parser) factor() *ParseNode {
	left := p.primary()

	if p.match([]int{MUL, DIV}) {
		operator := p.previous()
		right := p.factor()
		return NewTreeNodeWith(operator, left, right)
	}

	return left
}

func (p *Parser) primary() *ParseNode {
	if p.match([]int{NUMBER}) {
		num := p.previous()
		return NewTreeNode(num)
	}

	if p.match([]int{LPAREN}) {
		expr := p.expression()
		p.consume(RPAREN, "Expected closing ')'")
		return expr
	}

	var error = ErrorToken(fmt.Sprintf("Error found on line %d\n", p.peek().linenum))
	p.errors = append(p.errors, error)

	return nil
}

//// PARSER UTILITIES ////

func (p *Parser) match(types []int) bool {
	for _, t := range types {
		if p.check(t) {
			p.advance()
			return true
		}
	}
	return false
}

// func (p *Parser) consume(t int, message string) Token {
// 	if p.check(t) {
// 		return p.tokens.pop()
// 		// return p.advance()
// 	}
//
// 	error := ErrorToken(message, p.current.linenum)
// 	p.errors = append(p.errors, error)
// 	return nil
// }

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
