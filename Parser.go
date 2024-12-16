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
			node = p.parse_construct()
		} else if p.match_types(next_token.token_type, TT_IDENT) {
			// `x int` OR `x int = 4`
			node = p.parse_value_dec()
		} else if p.match_types(next_token.token_type, TT_ASSIGN) {
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

	type_identifier_node := p.parse_type()
	type_identifier_token := type_identifier_node.get_token()

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

func (p *Parser) parse_value_dec_list() ParseNode {
	if !p.match_types(p.current.token_type, TT_IDENT) {
		return nil
	}
	list_node := &BlockNode{}
	var list_token_lexeme string
	list_token := Token{TT_COMPILER_CREATED, "", p.current.linenum}

	for p.match_types(p.current.token_type, TT_IDENT) {
		value_dec := p.parse_value_dec()
		if value_dec.get_nodetype() == TRINARY {
			// NOTE: Maybe this could be a compiler warning and we just omit the assigned value
			p.raise_error("Cannot initialize value in value declaration list")
			return nil
		}

		if p.match_types(p.current.token_type, TT_RBRACE) || p.match_types(p.current.token_type, TT_RPAREN) {
			list_token.lexeme += value_dec.get_token().lexeme
			list_node.children = append(list_node.children, value_dec)
			break
		}

		p.consume(TT_COMMA, "Expected comma symbol after value declaration value declaration list\n")
		list_token_lexeme += fmt.Sprintf("%s, ", value_dec.get_token().lexeme)
		list_node.children = append(list_node.children, value_dec)
	}

	list_token.lexeme = list_token_lexeme
	list_node.token = &list_token

	return list_node
}

func (p *Parser) parse_assign() ParseNode {
	name_token := p.current
	name_node := NewUnaryNode(&name_token, IDENTIFIER)
	p.advance() // advance past the current identifier

	p.advance() // advance past the "=" symbol

	value_node := p.expression()

	return NewBinaryNode(&Token{}, ASSIGN, name_node, value_node)
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
		return p.parse_if_statement()
	case "while":
		return p.parse_while_statement()
	case "for":
		return p.parse_for_statement()
	case "include": // should throw an error!! (include not allowed in body of code)
		p.raise_error("Cannot include libraries during runtime !!\n")
		return nil
	}

	p.raise_error("Could not recognize statement\n")
	return nil
}

func (p *Parser) parse_if_statement() ParseNode {
	p.advance() // advance past the "if" keyword

	var elif_statements []ParseNode

	condition_expression := p.expression()
	condition_body := p.parse_body()
	prime_node_token := &Token{TT_COMPILER_CREATED, "if ... { ... }", condition_expression.get_token().linenum}
	primary_if := NewBinaryNode(prime_node_token, IF_BRANCH, condition_expression, condition_body)

	elif_statements = append(elif_statements, primary_if)

	// check for elif statements
	for p.current.lexeme == "else" {
		p.advance()

		if p.current.lexeme != "if" && p.current.lexeme != "{" {
			p.raise_error("Expected body or if statements after else keyword\n")
			return nil
		}

		if p.current.lexeme == "if" {
			p.advance()

			condition_exp := p.expression()
			condition_bod := p.parse_body()
			// TODO: more detailed token lexeme
			node_token := &Token{TT_COMPILER_CREATED, "else if ... { ... }", condition_exp.get_token().linenum}
			next_elif_statement := NewBinaryNode(node_token, IF_BRANCH, condition_exp, condition_bod)
			elif_statements = append(elif_statements, next_elif_statement)
		} else if p.match_types(p.current.token_type, TT_LBRACE) {
			p.advance()

			else_body := p.parse_body()
			node_token := &Token{TT_COMPILER_CREATED, "else { ... }", else_body.get_token().linenum}
			new_node := NewBranchUnaryNode(node_token, ELSE_BRANCH, else_body)
			elif_statements = append(elif_statements, new_node)
			break
		}
	}

	if_statement := &BlockNode{condition_expression.get_token(), IF, elif_statements}
	return if_statement
}

func (p *Parser) parse_while_statement() ParseNode {
	p.consume(TT_IDENT, "Expected 'while' keyword in while statement\n")

	condition_node := p.expression()

	body_node := p.parse_body()

	return NewBinaryNode(
		&Token{TT_COMPILER_CREATED, "while <condition> {...}", condition_node.get_token().linenum},
		WHILE,
		condition_node,
		body_node,
	)
}

func (p *Parser) parse_for_statement() ParseNode {
	p.consume(TT_IDENT, "Expected 'for' keyword in for statement\n")

	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("For statement expects iterator name\n")
		return nil
	}

	iterator_variable_token := p.current
	iterator_variable_node := NewUnaryNode(&iterator_variable_token, VAR_DEC)
	p.advance()

	if p.current.lexeme != "in" {
		p.raise_error("Expected 'in' keyword in for statement\n")
		return nil
	}
	p.advance()

	range_node := p.parse_range()

	body_node := p.parse_body()

	for_token := &Token{
		TT_COMPILER_CREATED,
		fmt.Sprintf("for %s in %s {...}", iterator_variable_token.lexeme, range_node.get_token().lexeme),
		iterator_variable_token.linenum,
	}
	return NewTrinaryNode(for_token, FOR, iterator_variable_node, range_node, body_node)
}

func (p *Parser) parse_range() ParseNode {
	lhs := p.expression()

	p.consume(TT_RANGE, "Expected '...' operator in range statement\n")

	rhs := p.expression()

	range_token := &Token{
		TT_COMPILER_CREATED,
		fmt.Sprintf("%s...%s", lhs.get_token().lexeme, rhs.get_token().lexeme),
		lhs.get_token().linenum,
	}
	return NewBinaryNode(range_token, RANGE, lhs, rhs)
}

func (p *Parser) parse_construct() ParseNode {
	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("Expected identifier name at the beginning of struct statement\n")
		return nil
	}

	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)

	p.advance()

	p.consume(TT_BINDING, "Expected '::' symbol in binding statement\n")

	var binding_node ParseNode
	var nodetype NodeType
	switch p.current.token_type {
	case TT_KEYWORD:
		// check if lexeme is 'struct'
		binding_node = p.parse_struct()
		nodetype = STRUCT
	case TT_LPAREN:
		// its a function
		binding_node = p.parse_function()
		nodetype = FUNCDEF
	default:
		binding_node = p.parse_type()
		nodetype = TYPE
	}

	construct_token := Token{}
	return NewBinaryNode(&construct_token, nodetype, name_identifier_node, binding_node)
}

func (p *Parser) parse_struct() ParseNode {
	if p.current.lexeme != "struct" {
		p.raise_error("Expected 'struct' keyword in structure defintion\n")
		return nil
	}

	p.consume(TT_LBRACE, "Expected LBRACE in structure definition\n")

	struct_contents := p.parse_value_dec_list()

	p.consume(TT_RBRACE, "Expected RBRACE in structure definintion\n")

	return NewBranchUnaryNode(&Token{}, STRUCT, struct_contents)
}

func (p *Parser) parse_function() ParseNode {
	p.consume(TT_LPAREN, "Expected LPAREN in function definition\n")

	signature_lexeme := "("
	parameter_list := p.parse_value_dec_list()
	signature_lexeme += parameter_list.get_token().lexeme

	p.consume(TT_RPAREN, "Expected RPAREN in function definition\n")
	p.consume(TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition\n")
	signature_lexeme += ") -> "

	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("Function must have a return type\n")
		return nil
	}

	return_type_token := p.current
	return_type_node := NewUnaryNode(&return_type_token, TYPE)
	signature_lexeme += return_type_token.lexeme
	p.advance()

	function_signature := &Token{TT_COMPILER_CREATED, signature_lexeme, parameter_list.get_token().linenum}

	if p.match_types(p.current.token_type, TT_NEWLINE) {
		p.advance()
		return NewBinaryNode(function_signature, FUNCDEF, parameter_list, return_type_node)
	}

	body_node := p.parse_body()

	return NewTrinaryNode(function_signature, FUNCDEF, parameter_list, return_type_node, body_node)
}

func (p *Parser) parse_body() ParseNode {
	p.consume(TT_LBRACE, "Expected LBRACE in body definition statement\n")
	body_token := Token{TT_COMPILER_CREATED, "{ ... }", p.current.linenum}
	body_node := &BlockNode{token: &body_token}

	for {
		// consume all newlines until we reach actual code
		for p.match_types(p.current.token_type, TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.token_type, TT_RBRACE) {
			break
		}

		next_statement := p.parse_statement()
		body_node.children = append(body_node.children, next_statement)
	}

	p.consume(TT_RBRACE, "Expected RBRACE in body definition statement\n")

	return body_node
}

func (p *Parser) parse_type() ParseNode {
	// parses for any of the different type structs: primitives, named, function types, array types, pointer types

	var return_node *UnaryNode
	if p.match_types(p.current.token_type, TT_LPAREN) {
		// function type
		function_type := p.parse_function()
		return_node = NewBranchUnaryNode(function_type.get_token(), TYPE, function_type)
		return_node.addtag("function_type_")

	} else if p.match_types(p.current.token_type, TT_LBRACKET) {
		// array type
		p.advance()

		array_type := p.parse_type()

		p.consume(TT_RBRACKET, "Expected RBRACKET in array type expression\n")

		return_node = NewBranchUnaryNode(array_type.get_token(), TYPE, array_type)
		return_node.addtag("array_of_")
	} else {
		type_token := p.current
		return_node = NewUnaryNode(&type_token, TYPE)
		return_node.addtag("named_type_")
		p.advance() // advance past the identifier
	}

	if p.match_types(p.current.token_type, TT_STAR) {
		p.advance() // advance past the star
		new_return_node := NewBranchUnaryNode(return_node.get_token(), TYPE, return_node)
		new_return_node.addtag("pointer_to_")
		return new_return_node
	}

	return return_node
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
