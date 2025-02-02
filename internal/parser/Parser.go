package sage

import (
	"fmt"
	sage "sage/internal/lexer"
	q "sage/internal/queues"
	t "sage/internal/tokens"
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
	current    t.Token
	node_cache ParseNode
	errors     []*t.Token
	errorCheck bool
}

func NewParser(filename string, file_contents []byte) *Parser {
	queue := q.NewQueue(file_contents)
	lexer := sage.NewLexer(filename, queue)

	return &Parser{
		lexer:      lexer,
		errors:     nil,
		errorCheck: false,
	}
}

func (p *Parser) Parse_program(debug_lexer bool) ParseNode {
	if debug_lexer {
		var curr_tok t.Token
		i := 0
		for curr_tok.Token_type != t.TT_EOF {
			curr_tok = p.lexer.Get_token()
			fmt.Printf("[%d] %+v\n", i, curr_tok)
			i++
		}

		return nil
	}

	p.current = p.lexer.Get_token()
	root := p.parse_libraries()
	program_root := p.statements()

	root.Children = append(root.Children, program_root.Children...)

	// check for errors to report
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
	p.consume(t.TT_KEYWORD, "Expected include keyword in include statement.\n")

	if p.current.Token_type != t.TT_STRING {
		p.raise_error("Expected string literal in include statement.\n")
		return nil
	}
	string_token := p.current
	node := NewUnaryNode(&string_token, INCLUDE)

	p.advance()
	p.consume(t.TT_NEWLINE, "Must have include statements on individual lines.\n")

	return node
}

func (p *Parser) parse_libraries() *BlockNode {
	list_node := ModuleRootNode()

	for len(p.errors) == 0 {
		for p.match_types(p.current.Token_type, t.TT_NEWLINE) {
			p.advance()
		}

		if !p.match_types(p.current.Token_type, t.TT_KEYWORD) {
			break
		}

		next_include := p.library_statement()
		list_node.Children = append(list_node.Children, next_include)
	}

	return list_node
}

func (p *Parser) statements() *BlockNode {
	list_node := ModuleRootNode()

	for len(p.errors) == 0 {
		// consume all newlines until we reach actul code
		for p.match_types(p.current.Token_type, t.TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.Token_type, t.TT_EOF) {
			break
		}

		next_statement := p.parse_statement()
		list_node.Children = append(list_node.Children, next_statement)
	}

	return list_node
}

func (p *Parser) parse_statement() ParseNode {
	switch p.current.Token_type {
	case t.TT_IDENT:
		next_token := p.peek()
		if p.match_types(next_token.Token_type, t.TT_BINDING) {
			// construct
			return p.parse_construct()
		} else if p.matches_any(next_token.Token_type, []t.TokenType{
			t.TT_KEYWORD, t.TT_LBRACKET, t.TT_IDENT,
		}) {
			// value declaration
			return p.parse_value_dec()
		} else if p.matches_any(next_token.Token_type, []t.TokenType{t.TT_ASSIGN, t.TT_FIELD_ACCESSOR}) {
			// value assignment
			retval := p.parse_assign()
			if retval == nil {
				// nil means that we actually were parsing an expression and should stop parsing for assign
				break
			}

			return retval
		} else if p.match_types(next_token.Token_type, t.TT_LPAREN) {
			return p.parse_function_call()
		}

	case t.TT_KEYWORD:
		// a keyword statement is found
		return p.parse_keyword_statement()
	case t.TT_POUND:
		return p.parse_compile_time_execute()
	}

	// otherwise it must be a normal expression
	return p.expression()
}

func (p *Parser) parse_compile_time_execute() ParseNode {
	p.consume(t.TT_POUND, "Expected 'run' statement to begin with '#' symbol\n")

	if p.current.Token_type != t.TT_IDENT || p.current.Lexeme != "run" {
		p.raise_error("Expected 'run' keyword in compile time execution statement\n")
		return nil
	}
	p.advance()

	run_token := t.NewToken(t.TT_COMPILER_CREATED, "#run { ... }", p.current.Linenum)
	run_body := p.parse_body()

	return NewBranchUnaryNode(&run_token, COMPILE_TIME_EXECUTE, run_body)
}

func (p *Parser) parse_value_dec() ParseNode {
	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance() // move past identifier

	if !p.matches_any(p.current.Token_type, []t.TokenType{t.TT_KEYWORD, t.TT_LBRACKET, t.TT_IDENT, t.TT_VARARG}) {
		p.raise_error("Expected type to be associated with identifier in value declaration statement\n")
		return nil
	}

	type_identifier_node := p.parse_type()
	if type_identifier_node == nil {
		p.raise_error("Found invalid type in value declaration list.")
		return nil
	}

	type_identifier_token := type_identifier_node.Get_token()

	if p.match_types(p.current.Token_type, t.TT_ASSIGN) {
		p.advance() // move past the "=" symbol
		rhs := p.expression()

		new_lexeme := fmt.Sprintf("%s %s = %s", name_identifier_token.Lexeme, type_identifier_token.Lexeme, rhs)

		dec_token := t.NewToken(t.TT_ASSIGN, new_lexeme, name_identifier_token.Linenum)
		return NewTrinaryNode(
			&dec_token,
			ASSIGN,
			name_identifier_node,
			type_identifier_node,
			rhs,
		)
	}

	dec_lexeme := fmt.Sprintf("%s %s", name_identifier_token.Lexeme, type_identifier_token.Lexeme)

	var nodetype NodeType = VAR_DEC
	if type_identifier_node.Get_true_nodetype() == VARARG {
		nodetype = VARARG
	}

	token := t.NewToken(t.TT_COMPILER_CREATED, dec_lexeme, name_identifier_token.Linenum)
	return &BinaryNode{
		token:    &token,
		Nodetype: nodetype,
		Left:     name_identifier_node,
		Right:    type_identifier_node,
	}
}

func (p *Parser) parse_value_dec_list() ParseNode {
	// first we check if the parameter list is empty, if it is an empty list then the first token in the list should be the ending ')' character
	if p.match_types(p.current.Token_type, t.TT_RPAREN) {
		return &BlockNode{
			token:    &t.Token{Token_type: t.TT_COMPILER_CREATED, Lexeme: "Empty Parameter List", Linenum: p.current.Linenum},
			Nodetype: PARAM_LIST,
			Children: []ParseNode{},
		}
	}

	// newlines before a value dec list are allowed
	for p.match_types(p.current.Token_type, t.TT_NEWLINE) {
		p.advance()
	}

	// otherwise we expected the list to be populated with var dec entries beginning with an identifier
	if !p.match_types(p.current.Token_type, t.TT_IDENT) {
		p.raise_error("Expecting value dec to begin with identifier\n")
		return nil
	}

	list_node := &BlockNode{}
	list_node.Nodetype = PARAM_LIST
	var list_token_lexeme string
	list_token := t.Token{Token_type: t.TT_COMPILER_CREATED, Lexeme: "", Linenum: p.current.Linenum}

	for p.match_types(p.current.Token_type, t.TT_IDENT) {
		value_dec := p.parse_value_dec()
		if value_dec.Get_nodetype() == TRINARY {
			// NOTE: Maybe this could be a compiler warning and we just omit the assigned value
			p.raise_error("Cannot initialize value in value declaration list\n")
			return nil
		}

		if p.matches_any(p.current.Token_type, []t.TokenType{t.TT_RBRACE, t.TT_RPAREN}) {
			list_token.Lexeme += value_dec.Get_token().Lexeme
			list_node.Children = append(list_node.Children, value_dec)
			break
		}

		p.consume(t.TT_COMMA, "Expected comma symbol after value declaration value declaration list\n")
		list_token_lexeme += fmt.Sprintf("%s, ", value_dec.Get_token().Lexeme)
		list_node.Children = append(list_node.Children, value_dec)

		// value declarations are allowed to be seperated by newline(s)
		for p.match_types(p.current.Token_type, t.TT_NEWLINE) {
			p.advance()
		}
	}

	list_token.Lexeme = list_token_lexeme
	list_node.token = &list_token

	return list_node
}

func (p *Parser) parse_assign() ParseNode {
	name_token := p.current
	var name_node ParseNode = NewUnaryNode(&name_token, IDENTIFIER)

	// this code section is here to handle field accessor cases and function call cases
	lookahead := p.peek()
	is_field_access := p.match_types(lookahead.Token_type, t.TT_FIELD_ACCESSOR)
	if is_field_access {
		// we could be assigning something new to a struct field
		name_node = p.parse_struct_field_access()
		temp_name_node := name_node.(*ListNode)
		name_token.Lexeme = temp_name_node.Get_full_lexeme()
	} else {
		// parse_struct_field_access leaves the current token cursor on the next token already
		// so if it is a field access then calling this p.advance() would be unnecessary
		p.advance() // advance past the current identifier
	}

	// it could be that we are referencing the field in some expression,
	if is_field_access && p.current.Token_type != t.TT_ASSIGN {
		p.set_node_cache(name_node) // save parsed node for further expression parsing
		return nil                  // return nil to signify that we actually are parsing a different kind of statement
	}
	/////

	p.consume(t.TT_ASSIGN, "Expected '=' symbol in assign statement\n")

	value_node := p.expression()

	token_lexeme := fmt.Sprintf("%s = %s", name_token.Lexeme, value_node.Get_token().Lexeme)
	token := t.NewToken(t.TT_ASSIGN, token_lexeme, name_token.Linenum)
	return NewBinaryNode(&token, ASSIGN, name_node, value_node)
}

func (p *Parser) parse_keyword_statement() ParseNode {
	switch p.current.Lexeme {
	case "ret":
		return_token := p.current
		lookahead := p.peek()
		if p.match_types(lookahead.Token_type, t.TT_NEWLINE) {
			p.advance() // advance past return token
			return NewUnaryNode(&return_token, KEYWORD)
		}

		p.advance() // advance past return token
		expression := p.expression()
		new_token := t.NewToken(t.TT_KEYWORD, fmt.Sprintf("ret %s", expression), return_token.Linenum)

		return NewBranchUnaryNode(&new_token, KEYWORD, expression)

	case "if":
		return p.parse_if_statement()
	case "while":
		return p.parse_while_statement()
	case "for":
		return p.parse_for_statement()
	}

	p.raise_error("Could not recognize statement\n")
	return nil
}

func (p *Parser) parse_if_statement() ParseNode {
	p.advance() // advance past the "if" keyword

	var elif_statements []ParseNode

	condition_expression := p.expression()
	condition_body := p.parse_body()
	prime_node_token := t.NewToken(t.TT_COMPILER_CREATED, "if ... { ... }", condition_expression.Get_token().Linenum)
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
			node_token := t.NewToken(t.TT_COMPILER_CREATED, "else if ... { ... }", condition_exp.Get_token().Linenum)
			next_elif_statement := NewBinaryNode(&node_token, IF_BRANCH, condition_exp, condition_bod)
			elif_statements = append(elif_statements, next_elif_statement)
		} else if p.match_types(p.current.Token_type, t.TT_LBRACE) {

			else_body := p.parse_body()
			node_token := t.NewToken(t.TT_COMPILER_CREATED, "else { ... }", else_body.Get_token().Linenum)
			new_node := NewBranchUnaryNode(&node_token, ELSE_BRANCH, else_body)
			elif_statements = append(elif_statements, new_node)
			break
		}
	}

	if_statement := &BlockNode{condition_expression.Get_token(), IF, elif_statements}
	return if_statement
}

func (p *Parser) parse_while_statement() ParseNode {
	p.consume(t.TT_KEYWORD, "Expected 'while' keyword in while statement\n")

	condition_node := p.expression()

	body_node := p.parse_body()

	token := t.NewToken(t.TT_COMPILER_CREATED, "while <condition> {...}", condition_node.Get_token().Linenum)
	return NewBinaryNode(
		&token,
		WHILE,
		condition_node,
		body_node,
	)
}

func (p *Parser) parse_for_statement() ParseNode {
	p.consume(t.TT_KEYWORD, "Expected 'for' keyword in for statement\n")

	if !p.match_types(p.current.Token_type, t.TT_IDENT) {
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

	for_token := t.NewToken(
		t.TT_COMPILER_CREATED,
		fmt.Sprintf("for %s in %s {...}", iterator_variable_token.Lexeme, range_node.Get_token().Lexeme),
		iterator_variable_token.Linenum,
	)
	return NewTrinaryNode(&for_token, FOR, iterator_variable_node, range_node, body_node)
}

func (p *Parser) parse_range() ParseNode {
	lhs := p.expression()

	p.consume(t.TT_RANGE, "Expected '...' operator in range statement\n")

	rhs := p.expression()

	range_token := t.NewToken(
		t.TT_COMPILER_CREATED,
		fmt.Sprintf("%s...%s", lhs.Get_token().Lexeme, rhs.Get_token().Lexeme),
		lhs.Get_token().Linenum,
	)

	return NewBinaryNode(&range_token, RANGE, lhs, rhs)
}

func (p *Parser) parse_construct() ParseNode {
	if !p.match_types(p.current.Token_type, t.TT_IDENT) {
		p.raise_error("Expected identifier name at the beginning of struct statement\n")
		return nil
	}

	name_identifier_token := p.current
	name_identifier_node := NewUnaryNode(&name_identifier_token, IDENTIFIER)
	p.advance() // move identifier

	p.consume(t.TT_BINDING, "Expected '::' symbol in binding statement\n")

	var binding_node ParseNode
	var nodetype NodeType
	switch p.current.Token_type {
	case t.TT_KEYWORD:
		// check if lexeme is 'struct'
		binding_node = p.parse_struct()
		nodetype = STRUCT
	case t.TT_LPAREN:
		// its a function
		binding_node = p.parse_function()
		nodetype = binding_node.Get_true_nodetype()
	default:
		binding_node = p.parse_type()
		nodetype = TYPE
	}

	if binding_node == nil {
		return nil
	}

	token_lexeme := fmt.Sprintf("%s :: %s", name_identifier_token.Lexeme, binding_node.Get_token().Lexeme)
	construct_token := t.NewToken(t.TT_COMPILER_CREATED, token_lexeme, name_identifier_token.Linenum)
	return NewBinaryNode(&construct_token, nodetype, name_identifier_node, binding_node)
}

func (p *Parser) parse_struct() ParseNode {
	if p.current.Lexeme != "struct" {
		p.raise_error("Expected 'struct' keyword in structure defintion\n")
		return nil
	}
	p.advance() // advance past the struct keyword

	p.consume(t.TT_LBRACE, "Expected LBRACE in structure definition\n")

	struct_contents := p.parse_value_dec_list()

	p.consume(t.TT_RBRACE, "Expected RBRACE in structure definintion\n")

	return NewBranchUnaryNode(struct_contents.Get_token(), STRUCT, struct_contents)
}

func (p *Parser) parse_function() ParseNode {
	p.consume(t.TT_LPAREN, "Expected LPAREN in function definition\n")

	signature_lexeme := "("
	parameter_list := p.parse_value_dec_list()
	if parameter_list == nil {
		return nil
	}

	signature_lexeme += parameter_list.Get_token().Lexeme

	p.consume(t.TT_RPAREN, "Expected RPAREN in function definition\n")
	p.consume(t.TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition\n")
	signature_lexeme += ") -> "

	if !p.match_types(p.current.Token_type, t.TT_KEYWORD) {
		p.raise_error("Function must have a return type\n")
		return nil
	}

	return_type_token := p.current
	return_type_node := NewUnaryNode(&return_type_token, TYPE)
	signature_lexeme += return_type_token.Lexeme
	p.advance() // move past type keyword

	function_signature := t.NewToken(t.TT_COMPILER_CREATED, signature_lexeme, parameter_list.Get_token().Linenum)

	// if there is no brace and instead if immidiately followed by a NEWLINE then there is no body
	if p.match_types(p.current.Token_type, t.TT_NEWLINE) {
		p.advance() // move past the newline
		return NewBinaryNode(&function_signature, FUNCDEC, parameter_list, return_type_node)
	}

	body_node := p.parse_body()

	return NewTrinaryNode(&function_signature, FUNCDEF, parameter_list, return_type_node, body_node)
}

func (p *Parser) parse_function_call() ParseNode {
	function_name_token := p.current
	p.advance() // move past func name identifier

	// next token should be a '('
	p.consume(t.TT_LPAREN, "Expected function call to include opening '(' bracket\n")

	var params []ParseNode
	for {
		param := p.parse_primary()
		params = append(params, param)

		if !p.match_types(p.current.Token_type, t.TT_COMMA) {
			break
		}
		p.advance()
	}

	p.consume(t.TT_RPAREN, "Expected function call to include closing ')' bracket\n")

	// create block node to hold params in
	params_token := t.NewToken(t.TT_COMPILER_CREATED, "", -1)
	params_node := NewBlockNode(&params_token, params)

	return NewBranchUnaryNode(&function_name_token, FUNCCALL, params_node)
}

func (p *Parser) parse_body() ParseNode {
	p.consume(t.TT_LBRACE, "Expected LBRACE in body definition statement\n")

	body_token := t.NewToken(t.TT_COMPILER_CREATED, "{ ... }", p.current.Linenum)
	body_node := &BlockNode{token: &body_token}

	for {
		// consume all newlines until we reach actual code
		for p.match_types(p.current.Token_type, t.TT_NEWLINE) {
			p.advance()
		}
		if p.match_types(p.current.Token_type, t.TT_RBRACE) {
			break
		}

		next_statement := p.parse_statement()
		body_node.Children = append(body_node.Children, next_statement)
	}

	p.consume(t.TT_RBRACE, "Expected RBRACE in body definition statement\n")

	body_node.Nodetype = BLOCK
	return body_node
}

func (p *Parser) parse_type() ParseNode {
	// parses for any of the different type structs: primitives, named, function types, array types, pointer types

	var return_node *UnaryNode
	// NOTE: function type
	// TODO: change this into a switch case
	if p.match_types(p.current.Token_type, t.TT_LPAREN) {
		function_type := p.parse_function()
		return_node = NewBranchUnaryNode(function_type.Get_token(), TYPE, function_type)
		return_node.addtag("function_type_") // add a tag to give further info about what kind of type it is

		return return_node // return now because pointers to function types aren't allowed

	} else if p.match_types(p.current.Token_type, t.TT_LBRACKET) { // NOTE: array type
		// keep track of how many dims this array has
		lbracket_nest_count := 0
		token_lexeme := ""
		for p.match_types(p.current.Token_type, t.TT_LBRACKET) {
			p.advance() // move past lbracket
			lbracket_nest_count++
			token_lexeme += "["
		}

		if !p.matches_any(p.current.Token_type, []t.TokenType{t.TT_KEYWORD, t.TT_IDENT}) {
			p.raise_error("Expected valid type identifier in array type\n")
			return nil
		}

		array_type_token := p.current
		array_type_node := NewUnaryNode(&array_type_token, TYPE)
		token_lexeme += array_type_token.Lexeme
		p.advance() // advance past the identifier

		if p.match_types(p.current.Token_type, t.TT_COLON) {
			p.advance() // advance past the colon

			if !p.match_types(p.current.Token_type, t.TT_NUM) {
				p.raise_error("Expected a number to define the length of the array in array type declaration\n")
				return nil
			}

			return_node.addtag(fmt.Sprintf("ARRAY_LENGTH: %s", p.current.Lexeme))
			p.advance() // advance past the number
		}

		// match the corresponding rbrackets after the type keyword
		for p.match_types(p.current.Token_type, t.TT_RBRACKET) {
			p.advance()
			lbracket_nest_count--
			token_lexeme += "]"
		}

		if lbracket_nest_count != 0 {
			p.raise_error("Unambiguous array nesting found, please ensure all '[' have a matching ']'\n")
			return nil
		}

		new_token := t.NewToken(array_type_token.Token_type, token_lexeme, array_type_token.Linenum)
		return_node = NewBranchUnaryNode(&new_token, TYPE, array_type_node)
	} else if p.match_types(p.current.Token_type, t.TT_VARARG) {
		p.advance() // advance past the vararg '...' notation
		type_token := p.current
		return_node = NewUnaryNode(&type_token, VARARG)
		return_node.addtag("named_type_")
		p.advance()

	} else {
		type_token := p.current
		return_node = NewUnaryNode(&type_token, TYPE)
		return_node.addtag("named_type_")
		p.advance() // advance past the identifier
	}

	if p.current.Lexeme == "*" {
		p.advance() // advance past the star
		retnode_token := return_node.Get_token()
		// new_token := &sage.Token{retnode_token.Token_type, fmt.Sprintf("%s*", retnode_token.Lexeme), retnode_token.Linenum}
		new_token := t.NewToken(retnode_token.Token_type, fmt.Sprintf("%s*", retnode_token.Lexeme), retnode_token.Linenum)
		new_return_node := NewBranchUnaryNode(&new_token, TYPE, return_node)
		new_return_node.addtag("pointer_to_")
		return new_return_node
	}

	return return_node
}

func (p *Parser) expression() ParseNode {
	// if the node cache isn't empty then that means we were previously parsing,
	//	  an identifier that looked like another statement but actually was an expression,
	//	  so instead of parsing for a primary we simply use the primary that was already found first.
	if p.node_cache != nil {
		first_primary := p.node_cache
		p.clear_node_cache()

		return p.parse_operator(first_primary, t.TT_EQUALITY)
	}
	return p.parse_operator(p.parse_primary(), t.TT_EQUALITY)
}

func (p *Parser) parse_operator(left ParseNode, min_precedence t.TokenType) ParseNode {
	current := p.current
	for is_operator(current.Token_type) && current.Token_type >= min_precedence {
		op := current
		p.advance()

		right := p.parse_primary()
		current = p.current
		for is_operator(current.Token_type) && current.Token_type > op.Token_type ||
			(p.op_is_right_associative(current.Lexeme) && current.Token_type == op.Token_type) {

			inc := decide_precedence_inc(current.Token_type, op.Token_type)
			right = p.parse_operator(right, t.TokenType(int(op.Token_type)+inc))

			current = p.current
		}
		left = NewBinaryNode(&op, BINARY, left, right)
	}

	return left
}

func (p *Parser) parse_primary() ParseNode {
	switch p.current.Token_type {
	case t.TT_NUM:
		token := p.current
		ret := NewUnaryNode(&token, NUMBER)
		p.advance()
		return ret

	case t.TT_FLOAT:
		token := p.current
		ret := NewUnaryNode(&token, FLOAT)
		p.advance()
		return ret

	case t.TT_IDENT:
		token := p.current
		lookahead := p.peek()
		if p.match_types(lookahead.Token_type, t.TT_FIELD_ACCESSOR) {
			return p.parse_struct_field_access()
		} else if p.match_types(lookahead.Token_type, t.TT_LPAREN) {
			return p.parse_function_call()
		}

		ret := NewUnaryNode(&token, VAR_REF)
		p.advance()
		return ret

	case t.TT_LPAREN:
		p.consume(t.TT_LPAREN, "Expected opening '('")
		expr := p.expression()
		p.consume(t.TT_RPAREN, "Expected closing ')'")
		return expr

	case t.TT_STRING:
		token := p.current
		ret := NewUnaryNode(&token, STRING)
		p.advance()
		return ret

	default:
		p.raise_error("Unrecognized statement; could not find valid primary\n")
		return nil
	}
}

func (p *Parser) parse_struct_field_access() ParseNode {
	var identifier_lexemes []string
	identifier_lexemes = append(identifier_lexemes, p.current.Lexeme)
	p.advance()

	for p.match_types(p.current.Token_type, t.TT_FIELD_ACCESSOR) {
		p.advance()
		if !p.match_types(p.current.Token_type, t.TT_IDENT) {
			p.raise_error("Expected identifier in struct field accessor statement\n")
			return nil
		}

		identifier_lexemes = append(identifier_lexemes, p.current.Lexeme)
		p.advance()
	}

	token := t.NewToken(t.TT_COMPILER_CREATED, identifier_lexemes[0], -1)
	return NewListNode(&token, LIST, identifier_lexemes)
}

func is_operator(tokentype t.TokenType) bool {
	// NOTE: 3 is the highest operator precedence enum value, everything after 3 is other token types
	return tokentype <= 3
}

func decide_precedence_inc(curr_op_type t.TokenType, op_type t.TokenType) int {
	if curr_op_type > op_type {
		return 1
	}
	return 0
}

//// PARSER UTILITIES ////

func (p *Parser) raise_error(message string) t.Token {
	error := t.NewErrorToken(message, p.current.Linenum)
	p.errors = append(p.errors, &error)
	return error
}

func (p *Parser) match_types(type_a t.TokenType, type_b t.TokenType) bool {
	return type_a == type_b
}

func (p *Parser) matches_any(type_a t.TokenType, possible_types []t.TokenType) bool {
	for _, type_i := range possible_types {
		if p.match_types(type_a, type_i) {
			return true
		}
	}
	return false
}

// if the current token is token_type then advance otherwise throw an error
// used to expected certain tokens in the input
func (p *Parser) consume(token_type t.TokenType, message string) t.Token {
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

func (p *Parser) peek() t.Token {
	retval := p.lexer.Get_token()
	p.lexer.Unget_token()
	return retval
}

// checks the current token against the type parameter
func (p *Parser) current_token_type_is(token_type t.TokenType) bool {
	if p.is_ending_token() {
		return false
	}

	return p.current.Token_type == token_type
}

func (p *Parser) is_ending_token() bool {
	return p.current.Token_type == t.TT_EOF
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

func (p *Parser) set_node_cache(node ParseNode) {
	p.node_cache = node
}

func (p *Parser) clear_node_cache() {
	p.node_cache = nil
}
