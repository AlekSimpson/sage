package sage

import (
	"fmt"
	"sage/internal/lexer"
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
	lexer      sage.Lexer
	current    sage.Token
	errors     []*sage.Token
	errorCheck bool
}

func NewParser(file_contents []byte) *Parser {
	queue := sage.NewQueue(file_contents, 0)
	lexer := sage.NewLexer(queue)

	return &Parser{
		lexer:      lexer,
		errors:     nil,
		errorCheck: false,
	}
}

func (p *Parser) Parse_program() ParseNode {
	// TODO: libraries := p.parse_libraries()
	p.current = p.lexer.Get_token()
	root := p.statements()
	// TODO: root.children = append(root.children, libraries.children...)

	if len(p.errors) != 0 {
		for _, error := range p.errors {
			fmt.Println(error.Lexeme)
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
		for p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
			p.advance()
		}
		if !p.match_types(p.current.Token_type, sage.TT_INCLUDE) {
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
		for p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.Token_type, sage.TT_EOF) {
			break
		}

		next_statement := p.parse_statement()
		list_node.children = append(list_node.children, next_statement)
	}

	return list_node
}

func (p *Parser) parse_statement() ParseNode {
	switch p.current.Token_type {
	case sage.TT_IDENT:
		next_token := p.peek()
		if p.match_types(next_token.Token_type, sage.TT_BINDING) {
			// construct
			return p.parse_construct()
		} else if p.matches_any(next_token.Token_type, []sage.TokenType{sage.TT_KEYWORD, sage.TT_LBRACKET, sage.TT_IDENT}) {
			// value declaration
			return p.parse_value_dec()
		} else if p.match_types(next_token.Token_type, sage.TT_ASSIGN) {
			// value assignment
			return p.parse_assign()
		}

	case sage.TT_KEYWORD:
		// a keyword statement is found
		return p.parse_keyword_statement()
	}

	// otherwise it must be a normal expression
	return p.expression()
}

func (p *Parser) parse_value_dec() ParseNode {
	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance() // move past identifier

	if !p.matches_any(p.current.Token_type, []sage.TokenType{sage.TT_KEYWORD, sage.TT_LBRACKET, sage.TT_IDENT}) {
		p.raise_error("Expected type to be associated with identifier in value declaration statement\n")
		return nil
	}

	type_identifier_node := p.parse_type()
	if type_identifier_node == nil {
		return nil
	}

	type_identifier_token := type_identifier_node.get_token()

	if p.match_types(p.current.Token_type, sage.TT_ASSIGN) {
		p.advance() // move past the "=" symbol
		rhs := p.expression()

		new_lexeme := fmt.Sprintf("%s %s = %s", name_identifier_token.Lexeme, type_identifier_token.Lexeme, rhs)

		dec_token := sage.NewToken(sage.TT_ASSIGN, new_lexeme, name_identifier_token.Linenum)
		return NewTrinaryNode(
			&dec_token,
			ASSIGN,
			name_identifier_node,
			type_identifier_node,
			rhs,
		)
	}

	dec_lexeme := fmt.Sprintf("%s %s", name_identifier_token.Lexeme, type_identifier_token.Lexeme)

	token := sage.NewToken(sage.TT_COMPILER_CREATED, dec_lexeme, name_identifier_token.Linenum)
	return &BinaryNode{
		token:    &token,
		nodetype: VAR_DEC,
		left:     name_identifier_node,
		right:    type_identifier_node,
	}
}

func (p *Parser) parse_value_dec_list() ParseNode {
	// first we check if the parameter list is empty, if it is an empty list then the first token in the list should be the ending ')' character
	if p.match_types(p.current.Token_type, sage.TT_RPAREN) {
		return &BlockNode{
			token:    &sage.Token{Token_type: sage.TT_COMPILER_CREATED, Lexeme: "Empty Parameter List", Linenum: p.current.Linenum},
			nodetype: PARAM_LIST,
			children: []ParseNode{},
		}
	}

	// newlines before a value dec list are allowed
	for p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
		p.advance()
	}

	// otherwise we expected the list to be populated with var dec entries beginning with an identifier
	if !p.match_types(p.current.Token_type, sage.TT_IDENT) {
		p.raise_error("Expecting value dec to begin with identifier\n")
		return nil
	}

	list_node := &BlockNode{}
	list_node.nodetype = PARAM_LIST
	var list_token_lexeme string
	list_token := sage.Token{Token_type: sage.TT_COMPILER_CREATED, Lexeme: "", Linenum: p.current.Linenum}

	for p.match_types(p.current.Token_type, sage.TT_IDENT) {
		value_dec := p.parse_value_dec()
		if value_dec.get_nodetype() == TRINARY {
			// NOTE: Maybe this could be a compiler warning and we just omit the assigned value
			p.raise_error("Cannot initialize value in value declaration list\n")
			return nil
		}

		if p.matches_any(p.current.Token_type, []sage.TokenType{sage.TT_RBRACE, sage.TT_RPAREN}) {
			list_token.Lexeme += value_dec.get_token().Lexeme
			list_node.children = append(list_node.children, value_dec)
			break
		}

		p.consume(sage.TT_COMMA, "Expected comma symbol after value declaration value declaration list\n")
		list_token_lexeme += fmt.Sprintf("%s, ", value_dec.get_token().Lexeme)
		list_node.children = append(list_node.children, value_dec)

		// value declarations are allowed to be seperated by newline(s)
		for p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
			p.advance()
		}
	}

	list_token.Lexeme = list_token_lexeme
	list_node.token = &list_token

	return list_node
}

func (p *Parser) parse_assign() ParseNode {
	name_token := p.current
	name_node := NewUnaryNode(&name_token, IDENTIFIER)
	p.advance() // advance past the current identifier

	p.consume(sage.TT_ASSIGN, "Expected '=' symbol in assign statement\n")

	value_node := p.expression()

	token_lexeme := fmt.Sprintf("%s = %s", name_token.Lexeme, value_node.get_token().Lexeme)
	token := sage.NewToken(sage.TT_ASSIGN, token_lexeme, name_token.Linenum)
	return NewBinaryNode(&token, ASSIGN, name_node, value_node)
}

func (p *Parser) parse_keyword_statement() ParseNode {
	switch p.current.Lexeme {
	case "ret":
		return_token := p.current
		lookahead := p.peek()
		if p.match_types(lookahead.Token_type, sage.TT_NEWLINE) {
			p.advance() // advance past return token
			return NewUnaryNode(&return_token, KEYWORD)
		}

		p.advance() // advance past return token
		expression := p.expression()
		new_token := sage.NewToken(sage.TT_KEYWORD, fmt.Sprintf("ret %s", expression), return_token.Linenum)

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
	prime_node_token := sage.NewToken(sage.TT_COMPILER_CREATED, "if ... { ... }", condition_expression.get_token().Linenum)
	primary_if := NewBinaryNode(&prime_node_token, IF_BRANCH, condition_expression, condition_body)

	elif_statements = append(elif_statements, primary_if)

	// check for elif statements
	for p.current.Lexeme == "else" {
		p.advance() // move past else

		if p.current.Lexeme != "if" && p.current.Lexeme != "{" {
			p.raise_error("Expected body or if statements after else keyword\n")
			return nil
		}

		if p.current.Lexeme == "if" {
			p.advance() // move past if

			condition_exp := p.expression()
			condition_bod := p.parse_body()
			// TODO: more detailed token lexeme
			node_token := sage.NewToken(sage.TT_COMPILER_CREATED, "else if ... { ... }", condition_exp.get_token().Linenum)
			next_elif_statement := NewBinaryNode(&node_token, IF_BRANCH, condition_exp, condition_bod)
			elif_statements = append(elif_statements, next_elif_statement)
		} else if p.match_types(p.current.Token_type, sage.TT_LBRACE) {

			else_body := p.parse_body()
			node_token := sage.NewToken(sage.TT_COMPILER_CREATED, "else { ... }", else_body.get_token().Linenum)
			new_node := NewBranchUnaryNode(&node_token, ELSE_BRANCH, else_body)
			elif_statements = append(elif_statements, new_node)
			break
		}
	}

	if_statement := &BlockNode{condition_expression.get_token(), IF, elif_statements}
	return if_statement
}

func (p *Parser) parse_while_statement() ParseNode {
	p.consume(sage.TT_KEYWORD, "Expected 'while' keyword in while statement\n")

	condition_node := p.expression()

	body_node := p.parse_body()

	token := sage.NewToken(sage.TT_COMPILER_CREATED, "while <condition> {...}", condition_node.get_token().Linenum)
	return NewBinaryNode(
		&token,
		WHILE,
		condition_node,
		body_node,
	)
}

func (p *Parser) parse_for_statement() ParseNode {
	p.consume(sage.TT_KEYWORD, "Expected 'for' keyword in for statement\n")

	if !p.match_types(p.current.Token_type, sage.TT_IDENT) {
		p.raise_error("For statement expects iterator name\n")
		return nil
	}

	iterator_variable_token := p.current
	iterator_variable_node := NewUnaryNode(&iterator_variable_token, VAR_DEC)
	p.advance() // move past iterator

	if p.current.Lexeme != "in" {
		p.raise_error("Expected 'in' keyword in for statement\n")
		return nil
	}
	p.advance() // move past in keyword

	range_node := p.parse_range()

	body_node := p.parse_body()

	for_token := sage.NewToken(
		sage.TT_COMPILER_CREATED,
		fmt.Sprintf("for %s in %s {...}", iterator_variable_token.Lexeme, range_node.get_token().Lexeme),
		iterator_variable_token.Linenum,
	)
	return NewTrinaryNode(&for_token, FOR, iterator_variable_node, range_node, body_node)
}

func (p *Parser) parse_range() ParseNode {
	lhs := p.expression()

	p.consume(sage.TT_RANGE, "Expected '...' operator in range statement\n")

	rhs := p.expression()

	range_token := sage.NewToken(
		sage.TT_COMPILER_CREATED,
		fmt.Sprintf("%s...%s", lhs.get_token().Lexeme, rhs.get_token().Lexeme),
		lhs.get_token().Linenum,
	)

	return NewBinaryNode(&range_token, RANGE, lhs, rhs)
}

func (p *Parser) parse_construct() ParseNode {
	if !p.match_types(p.current.Token_type, sage.TT_IDENT) {
		p.raise_error("Expected identifier name at the beginning of struct statement\n")
		return nil
	}

	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance() // move identifier

	p.consume(sage.TT_BINDING, "Expected '::' symbol in binding statement\n")

	var binding_node ParseNode
	var nodetype NodeType
	switch p.current.Token_type {
	case sage.TT_KEYWORD:
		// check if lexeme is 'struct'
		binding_node = p.parse_struct()
		nodetype = STRUCT
	case sage.TT_LPAREN:
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

	token_lexeme := fmt.Sprintf("%s :: %s", name_identifier_token.Lexeme, binding_node.get_token().Lexeme)
	construct_token := sage.NewToken(sage.TT_COMPILER_CREATED, token_lexeme, name_identifier_token.Linenum)
	// construct_token := sage.Token{sage.TT_COMPILER_CREATED, token_lexeme, name_identifier_token.Linenum}
	return NewBinaryNode(&construct_token, nodetype, name_identifier_node, binding_node)
}

func (p *Parser) parse_struct() ParseNode {
	if p.current.Lexeme != "struct" {
		p.raise_error("Expected 'struct' keyword in structure defintion\n")
		return nil
	}
	p.advance() // advance past the struct keyword

	p.consume(sage.TT_LBRACE, "Expected LBRACE in structure definition\n")

	struct_contents := p.parse_value_dec_list()

	p.consume(sage.TT_RBRACE, "Expected RBRACE in structure definintion\n")

	return struct_contents
}

func (p *Parser) parse_function() ParseNode {
	p.consume(sage.TT_LPAREN, "Expected LPAREN in function definition\n")

	signature_lexeme := "("
	parameter_list := p.parse_value_dec_list()
	if parameter_list == nil {
		return nil
	}

	signature_lexeme += parameter_list.get_token().Lexeme

	p.consume(sage.TT_RPAREN, "Expected RPAREN in function definition\n")
	p.consume(sage.TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition\n")
	signature_lexeme += ") -> "

	if !p.match_types(p.current.Token_type, sage.TT_KEYWORD) {
		p.raise_error("Function must have a return type\n")
		return nil
	}

	return_type_token := p.current
	return_type_node := NewUnaryNode(&return_type_token, TYPE)
	signature_lexeme += return_type_token.Lexeme
	p.advance() // move past type keyword

	function_signature := sage.NewToken(sage.TT_COMPILER_CREATED, signature_lexeme, parameter_list.get_token().Linenum)
	// function_signature := &sage.Token{sage.TT_COMPILER_CREATED, signature_lexeme, parameter_list.get_token().Linenum}

	if p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
		p.advance() // move past the newline, NOTE: !!!! it might turn out that this advance breaks parsing flow, might need to remove it but I'm just going to leave it for now until it starts causing problems !!!!
		return NewBinaryNode(&function_signature, FUNCDEF, parameter_list, return_type_node)
	}

	body_node := p.parse_body()

	return NewTrinaryNode(&function_signature, FUNCDEF, parameter_list, return_type_node, body_node)
}

func (p *Parser) parse_body() ParseNode {
	p.consume(sage.TT_LBRACE, "Expected LBRACE in body definition statement\n")

	body_token := sage.NewToken(sage.TT_COMPILER_CREATED, "{ ... }", p.current.Linenum)
	body_node := &BlockNode{token: &body_token}

	for {
		// consume all newlines until we reach actual code
		for p.match_types(p.current.Token_type, sage.TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.Token_type, sage.TT_RBRACE) {
			break
		}

		next_statement := p.parse_statement()
		body_node.children = append(body_node.children, next_statement)
	}

	p.consume(sage.TT_RBRACE, "Expected RBRACE in body definition statement\n")

	return body_node
}

func (p *Parser) parse_type() ParseNode {
	// parses for any of the different type structs: primitives, named, function types, array types, pointer types

	var return_node *UnaryNode
	if p.match_types(p.current.Token_type, sage.TT_LPAREN) {
		// function type
		function_type := p.parse_function()
		return_node = NewBranchUnaryNode(function_type.get_token(), TYPE, function_type)
		return_node.addtag("function_type_") // add a tag to give further info about what kind of type it is

		return return_node // return now because pointers to function types aren't allowed

	} else if p.match_types(p.current.Token_type, sage.TT_LBRACKET) {
		// array type

		// keep track of how many dims this array has
		lbracket_nest_count := 0
		token_lexeme := ""
		for p.match_types(p.current.Token_type, sage.TT_LBRACKET) {
			p.advance() // move past lbracket
			lbracket_nest_count++
			token_lexeme += "["
		}

		if !p.matches_any(p.current.Token_type, []sage.TokenType{sage.TT_KEYWORD, sage.TT_IDENT}) {
			p.raise_error("Expected valid type identifier in array type\n")
			return nil
		}

		array_type_token := p.current
		array_type_node := NewUnaryNode(&array_type_token, TYPE)
		token_lexeme += array_type_token.Lexeme
		p.advance() // advance past the identifier

		// match the corresponding rbrackets after the type keyword
		for p.match_types(p.current.Token_type, sage.TT_RBRACKET) {
			p.advance()
			lbracket_nest_count--
			token_lexeme += "]"
		}

		if lbracket_nest_count != 0 {
			p.raise_error("Unambiguous array nesting found, please ensure all '[' have a matching ']'\n")
			return nil
		}

		new_token := &sage.Token{array_type_token.Token_type, token_lexeme, array_type_token.Linenum}
		return_node = NewBranchUnaryNode(new_token, TYPE, array_type_node)
		return_node.addtag("array_of_")
	} else {
		type_token := p.current
		return_node = NewUnaryNode(&type_token, TYPE)
		return_node.addtag("named_type_")
		p.advance() // advance past the identifier
	}

	if p.current.Lexeme == "*" {
		p.advance() // advance past the star
		retnode_token := return_node.get_token()
		new_token := &sage.Token{retnode_token.Token_type, fmt.Sprintf("%s*", retnode_token.Lexeme), retnode_token.Linenum}
		new_return_node := NewBranchUnaryNode(new_token, TYPE, return_node)
		new_return_node.addtag("pointer_to_")
		return new_return_node
	}

	return return_node
}

func (p *Parser) expression() ParseNode {
	return p.parse_operator(p.parse_primary(), sage.TT_EQUALITY)
}

func (p *Parser) parse_operator(left ParseNode, min_precedence sage.TokenType) ParseNode {
	current := p.current
	for is_operator(current.Token_type) && current.Token_type >= min_precedence {
		op := current
		p.advance()

		right := p.parse_primary()
		current = p.current
		for is_operator(current.Token_type) && current.Token_type > op.Token_type ||
			(p.op_is_right_associative(current.Lexeme) && current.Token_type == op.Token_type) {

			inc := decide_precedence_inc(current.Token_type, op.Token_type)
			right = p.parse_operator(right, sage.TokenType(int(op.Token_type)+inc))

			current = p.current
		}
		left = NewBinaryNode(&op, BINARY, left, right)
	}

	return left
}

func (p *Parser) parse_primary() ParseNode {
	switch p.current.Token_type {
	case sage.TT_NUM:
		token := p.current
		ret := NewUnaryNode(&token, NUMBER)
		p.advance()
		return ret

	case sage.TT_IDENT:
		// TODO: parse function calls
		token := p.current
		ret := NewUnaryNode(&token, VAR_REF)
		p.advance()
		return ret

	case sage.TT_LPAREN:
		p.consume(sage.TT_LPAREN, "Expected opening '('")
		expr := p.expression()
		p.consume(sage.TT_RPAREN, "Expected closing ')'")
		return expr

	case sage.TT_STRING:
		token := p.current
		ret := NewUnaryNode(&token, STRING)
		p.advance()
		return ret

	default:
		p.raise_error("Unrecognized statement; could not find valid primary\n")
		return nil
	}
}

func is_operator(tokentype sage.TokenType) bool {
	// NOTE: 3 is the highest operator precedence enum value, everything after 3 is other token types
	return tokentype <= 3
}

func decide_precedence_inc(curr_op_type sage.TokenType, op_type sage.TokenType) int {
	if curr_op_type > op_type {
		return 1
	}
	return 0
}

//// PARSER UTILITIES ////

func (p *Parser) raise_error(message string) sage.Token {
	error := sage.NewErrorToken(message, p.current.Linenum)
	p.errors = append(p.errors, &error)
	return error
}

func (p *Parser) match_types(type_a sage.TokenType, type_b sage.TokenType) bool {
	return type_a == type_b
}

func (p *Parser) matches_any(type_a sage.TokenType, possible_types []sage.TokenType) bool {
	for _, type_i := range possible_types {
		if p.match_types(type_a, type_i) {
			return true
		}
	}
	return false
}

// if the current token is token_type then advance otherwise throw an error
// used to expected certain tokens in the input
func (p *Parser) consume(token_type sage.TokenType, message string) sage.Token {
	if p.current_token_type_is(token_type) {
		p.advance()
		return p.current
	}

	error := p.raise_error(message)
	return error
}

func (p *Parser) advance() {
	p.current = p.lexer.Get_token()
}

func (p *Parser) peek() sage.Token {
	retval := p.lexer.Get_token()
	p.lexer.Unget_token()
	return retval
}

// checks the current token against the type parameter
func (p *Parser) current_token_type_is(token_type sage.TokenType) bool {
	if p.is_ending_token() {
		return false
	}

	return p.current.Token_type == token_type
}

func (p *Parser) is_ending_token() bool {
	return p.current.Token_type == sage.TT_EOF
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
