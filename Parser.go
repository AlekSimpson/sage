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
	libraries := p.parse_libraries()
	root := p.statements()
	root.children = append(root.children, libraries.children...)

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.lexeme)
		}
		p.errorCheck = true
	}

	return root
}

func (p *Parser) library_statement() ParseNode {
	// TODO:
	return nil
}

func (p *Parser) parse_libraries() *BlockNode {
	list_node := ModuleRootNode()

	for {
		for p.match_types(p.current.token_type, TT_NEWLINE) {
			p.advance()
		}
		if !p.match_types(p.current.token_type, TT_INCLUDE) {
			break
		}

		next_include := p.library_statement()
		list_node.children = append(list_node.children, next_include)
	}

	return list_node
}

func (p *Parser) statements() *BlockNode {
	list_node := ModuleRootNode()

	for {
		// consume all newlines until we reach actual code
		for p.match_types(p.current.token_type, TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.token_type, TT_EOF) {
			break
		}

		next_statement := p.parse_statement()
		list_node.children = append(list_node.children, next_statement)
	}

	return list_node
}

func (p *Parser) parse_statement() ParseNode {
	switch p.current.token_type {
	// parse value_dec, assign and construct statements
	case TT_IDENT:
		next_token := p.peek(1)
		var node ParseNode
		if p.match_types(next_token.token_type, TT_BINDING) {
			// construct
			node = p.parse_binding()
		}else if p.match_types(next_token.token_type, TT_IDENT) {
			// `x int` OR `x int = 4`
			node = p.parse_value_dec()
		}else if p.match_types(next_token.token_type, TT_ASSIGN) {
			node = p.parse_assign()
		}
		return node

	// TODO: parse keyword statement types
	case TT_KEYWORD:
		return p.parse_keyword_statement()

	default:
		return p.expression()

	}
}

func (p *Parser) parse_value_dec() ParseNode {
	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance()

	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("Expected type to be associated with identifier in value declaration statement\n")
		return nil
	}

	type_identifier_token := p.current
	type_identifier_node := NewUnaryNode(&type_identifier_token, IDENTIFIER)
	p.advance()

	if p.match_types(p.current.token_type, TT_ASSIGN) {
		p.advance() // move past the "=" symbol
		rhs := p.expression()

		new_lexeme := fmt.Sprintf("%s %s = %s", name_identifier_token.lexeme, type_identifier_token.lexeme, rhs.print())
		dec_token := &Token{TT_ASSIGN, new_lexeme, name_identifier_token.linenum}
		return NewTrinaryNode(
			dec_token, 
			ASSIGN,
			name_identifier_node, 
			type_identifier_node, 
			rhs,
		)
	}

	dec_lexeme := fmt.Sprintf("%s %s", name_identifier_token.lexeme, type_identifier_token.lexeme)

	return &BinaryNode{
		token:    &Token{TT_COMPILER_CREATED, dec_lexeme, name_identifier_token.linenum},
		nodetype: VAR_DEC,
		left:     name_identifier_node,
		right:    type_identifier_node,
	}
}

func (p *Parser) parse_assign() ParseNode {
	name_token := p.current
	name_node := NewUnaryNode(&name_token, IDENTIFIER)

	p.advance() // advance past the current identifier
	p.advance() // advance past the "=" symbol

	value_node := p.expression()

	return NewBinaryNode(&Token{}, ASSIGN, name_node, value_node)
}

func (p *Parser) parse_binding() ParseNode {
	// TODO:
	return nil
}

func (p *Parser) parse_keyword_statement() ParseNode {
	switch p.current.lexeme {
	case "ret":
		return_token := p.current
		lookahead := p.peek(1)
		if p.match_types(lookahead.token_type, TT_NEWLINE) {
			p.advance() // advance past return token
			return NewUnaryNode(&return_token, KEYWORD)
		}

		p.advance() // advance past return token
		expression := p.expression()
		new_token := Token{
			TT_KEYWORD, 
			fmt.Sprintf("ret %s", expression.print()), 
			return_token.linenum,
		}
		return NewUnaryNode(&new_token, KEYWORD)

	case "if":
	case "while":
	case "for":
	case "include": // should throw an error!! (include not allowed in body of code)
		p.raise_error("Cannot include libraries during runtime !!")
		return nil
	case "struct":
	}
	return nil
}

func (p *Parser) expression() ParseNode {
	return p.parse_operator(p.parse_primary(), TT_EQUALITY)
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
		left = NewBinaryNode(&op, BINARY, left, right)
	}

	return left
}

func (p *Parser) parse_primary() ParseNode {
	if p.match_types(p.current.token_type, TT_NUM) {
		token := p.current
		ret := NewUnaryNode(&token, NUMBER)
		p.advance() // get the number that was matched and update current with the next token in the queue
		return ret
	}

	if p.match_types(p.current.token_type, TT_LPAREN) {
		p.consume(TT_LPAREN, "Expected closing ')'")
		expr := p.expression()
		p.consume(TT_RPAREN, "Expected closing ')'")

		return expr
	}

	p.raise_error("Could not find valid primary\n")
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

func (p *Parser) raise_error(message string) Token {
	error := ErrorToken(message, p.current.linenum)
	p.errors = append(p.errors, error)
	return *error
}

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

	error := p.raise_error(message)
	return error
}

func (p *Parser) advance() {
	// if the current token index is already at the end
	//   then cap the index at the end of the array
	if p.current_token_index == len(p.tokens)-1 {
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
	return p.current.token_type == TT_EOF
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
