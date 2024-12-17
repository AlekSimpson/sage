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
	lexer      Lexer
	current    Token
	errors     []*Token
	errorCheck bool
}

func NewParser(lexer Lexer) *Parser {
	return &Parser{
		lexer:      lexer,
		errors:     nil,
		errorCheck: false,
	}
}

func (p *Parser) parse_program() ParseNode {
	// TODO: libraries := p.parse_libraries()
	p.current = p.lexer.get_token()
	root := p.statements()
	// TODO: root.children = append(root.children, libraries.children...)

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.lexeme)
		}
		p.errorCheck = true

		return nil
	}

	return root
}

func (p *Parser) library_statement() ParseNode {
	// TODO:
	return nil
}

func (p *Parser) parse_libraries() *BlockNode {
	// TODO:
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

	for len(p.errors) == 0 {
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
	case TT_IDENT:
		next_token := p.peek()
		if p.match_types(next_token.token_type, TT_BINDING) {
			// construct
			return p.parse_construct()
		} else if p.match_types(next_token.token_type, TT_KEYWORD) {
			// `x int` OR `x int = 4`
			return p.parse_value_dec()
		} else if p.match_types(next_token.token_type, TT_ASSIGN) {
			return p.parse_assign()
		}

	case TT_KEYWORD:
		return p.parse_keyword_statement()
	}

	return p.expression()
}

func (p *Parser) parse_value_dec() ParseNode {
	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance() // move past identifier

	if !p.match_types(p.current.token_type, TT_KEYWORD) {
		p.raise_error("Expected type to be associated with identifier in value declaration statement\n")
		return nil
	}

	type_identifier_node := p.parse_type()
	type_identifier_token := type_identifier_node.get_token()

	if p.match_types(p.current.token_type, TT_ASSIGN) {
		p.advance() // move past the "=" symbol
		rhs := p.expression()

		new_lexeme := fmt.Sprintf("%s %s = %s", name_identifier_token.lexeme, type_identifier_token.lexeme, rhs)
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
	// first we check if the parameter list is empty, if it is an empty list then the first token in the list should be the ending ')' character
	if p.match_types(p.current.token_type, TT_RPAREN) {
		return &BlockNode{
			token:    &Token{TT_COMPILER_CREATED, "Empty Parameter List", p.current.linenum},
			nodetype: PARAM_LIST,
			children: []ParseNode{},
		}
	}

	// newlines before a value dec list are allowed
	for p.match_types(p.current.token_type, TT_NEWLINE) {
		p.advance()
	}

	// otherwise we expected the list to be populated with var dec entries beginning with an identifier
	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("Expecting value dec to begin with identifier\n")
		return nil
	}

	list_node := &BlockNode{}
	list_node.nodetype = PARAM_LIST
	var list_token_lexeme string
	list_token := Token{TT_COMPILER_CREATED, "", p.current.linenum}

	for p.match_types(p.current.token_type, TT_IDENT) {
		value_dec := p.parse_value_dec()
		if value_dec.get_nodetype() == TRINARY {
			// NOTE: Maybe this could be a compiler warning and we just omit the assigned value
			p.raise_error("Cannot initialize value in value declaration list\n")
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

		// value declarations are allowed to be seperated by newline(s)
		for p.match_types(p.current.token_type, TT_NEWLINE) {
			p.advance()
		}
	}

	list_token.lexeme = list_token_lexeme
	list_node.token = &list_token

	return list_node
}

func (p *Parser) parse_assign() ParseNode {
	name_token := p.current
	name_node := NewUnaryNode(&name_token, IDENTIFIER)
	p.advance() // advance past the current identifier

	p.consume(TT_ASSIGN, "Expected '=' symbol in assign statement\n")

	value_node := p.expression()

	token_lexeme := fmt.Sprintf("%s = %s", name_token.lexeme, value_node.get_token().lexeme)
	return NewBinaryNode(&Token{TT_ASSIGN, token_lexeme, name_token.linenum}, ASSIGN, name_node, value_node)
}

func (p *Parser) parse_keyword_statement() ParseNode {
	switch p.current.lexeme {
	case "ret":
		return_token := p.current
		lookahead := p.peek()
		if p.match_types(lookahead.token_type, TT_NEWLINE) {
			p.advance() // advance past return token
			return NewUnaryNode(&return_token, KEYWORD)
		}

		p.advance() // advance past return token
		expression := p.expression()
		new_token := Token{
			TT_KEYWORD,
			fmt.Sprintf("ret %s", expression),
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
		p.advance() // move past else

		if p.current.lexeme != "if" && p.current.lexeme != "{" {
			p.raise_error("Expected body or if statements after else keyword\n")
			return nil
		}

		if p.current.lexeme == "if" {
			p.advance() // move past if

			condition_exp := p.expression()
			condition_bod := p.parse_body()
			// TODO: more detailed token lexeme
			node_token := &Token{TT_COMPILER_CREATED, "else if ... { ... }", condition_exp.get_token().linenum}
			next_elif_statement := NewBinaryNode(node_token, IF_BRANCH, condition_exp, condition_bod)
			elif_statements = append(elif_statements, next_elif_statement)
		} else if p.match_types(p.current.token_type, TT_LBRACE) {

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
	p.consume(TT_KEYWORD, "Expected 'while' keyword in while statement\n")

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
	p.consume(TT_KEYWORD, "Expected 'for' keyword in for statement\n")

	if !p.match_types(p.current.token_type, TT_IDENT) {
		p.raise_error("For statement expects iterator name\n")
		return nil
	}

	iterator_variable_token := p.current
	iterator_variable_node := NewUnaryNode(&iterator_variable_token, VAR_DEC)
	p.advance() // move past iterator

	if p.current.lexeme != "in" {
		p.raise_error("Expected 'in' keyword in for statement\n")
		return nil
	}
	p.advance() // move past in keyword

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
	p.advance() // move identifier

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

	if binding_node == nil {
		return nil
	}

	token_lexeme := fmt.Sprintf("%s :: %s", name_identifier_token.lexeme, binding_node.get_token().lexeme)
	construct_token := Token{TT_COMPILER_CREATED, token_lexeme, name_identifier_token.linenum}
	return NewBinaryNode(&construct_token, nodetype, name_identifier_node, binding_node)
}

func (p *Parser) parse_struct() ParseNode {
	if p.current.lexeme != "struct" {
		p.raise_error("Expected 'struct' keyword in structure defintion\n")
		return nil
	}
	p.advance() // advance past the struct keyword

	p.consume(TT_LBRACE, "Expected LBRACE in structure definition\n")

	struct_contents := p.parse_value_dec_list()

	p.consume(TT_RBRACE, "Expected RBRACE in structure definintion\n")

	return struct_contents
}

func (p *Parser) parse_function() ParseNode {
	p.consume(TT_LPAREN, "Expected LPAREN in function definition\n")

	signature_lexeme := "("
	parameter_list := p.parse_value_dec_list()
	if parameter_list == nil {
		return nil
	}

	signature_lexeme += parameter_list.get_token().lexeme

	p.consume(TT_RPAREN, "Expected RPAREN in function definition\n")
	p.consume(TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition\n")
	signature_lexeme += ") -> "

	if !p.match_types(p.current.token_type, TT_KEYWORD) {
		p.raise_error("Function must have a return type\n")
		return nil
	}

	return_type_token := p.current
	return_type_node := NewUnaryNode(&return_type_token, TYPE)
	signature_lexeme += return_type_token.lexeme
	p.advance() // move past type keyword

	function_signature := &Token{TT_COMPILER_CREATED, signature_lexeme, parameter_list.get_token().linenum}

	if p.match_types(p.current.token_type, TT_NEWLINE) {
		p.advance() // move past the newline, NOTE: !!!! it might turn out that this advance breaks parsing flow, might need to remove it but I'm just going to leave it for now until it starts causing problems !!!!
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
		p.advance() // move past lbracket

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

	if p.match_types(p.current.token_type, TT_MUL) {
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
	switch p.current.token_type {
	case TT_NUM:
		token := p.current
		ret := NewUnaryNode(&token, NUMBER)
		p.advance()
		return ret

	case TT_IDENT:
		// TODO: parse function calls
		token := p.current
		ret := NewUnaryNode(&token, VAR_REF)
		p.advance()
		return ret

	case TT_LPAREN:
		p.consume(TT_LPAREN, "Expected opening '('")
		expr := p.expression()
		p.consume(TT_RPAREN, "Expected closing ')'")
		return expr

	default:
		p.raise_error("Could not find valid primary\n")
		return nil
	}
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
	error := NewErrorToken(message, p.current.linenum)
	p.errors = append(p.errors, &error)
	return error
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
	p.current = p.lexer.get_token()
}

func (p *Parser) peek() Token {
	retval := p.lexer.get_token()
	p.lexer.unget_token()
	return retval
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
