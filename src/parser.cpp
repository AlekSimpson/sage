#include "../include/lexer.h"
#include "../include/node_manager.h"
#include "../include/token.h"
#include "../include/parser.h"

#include <unordered_map>
#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <string>
using namespace std;

bool is_operator(Token op);
int decide_precedence_inc(TokenType curr_op_type, TokenType op_type);

SageParser::SageParser(){}

SageParser::SageParser(NodeManager* _node_manager, string _filename) {
    lexer = new SageLexer(_filename);

    node_manager = _node_manager;
    filename = _filename;
    current_token = nullptr;
    node_cache = NULL_INDEX;
    errors = vector<Token>();
}

SageParser::~SageParser() {
    if (this->lexer != nullptr) {
        delete this->lexer;
    }
}

NodeIndex SageParser::parse_program(bool debug_lexer) {
    current_token = lexer->get_token();

    if (debug_lexer) {
        Token* walk_token = current_token;
        int counter = 0;
        while (walk_token != nullptr && walk_token->token_type != TT_EOF) {
            printf("[%d] %s\n", counter, walk_token->to_string().c_str());
            current_token = lexer->get_token();
            counter++;
        }

        return NULL_INDEX;
    }

    NodeIndex program_root = parse_statements();

    if (errors.size() != 0) {
        for (Token err : errors) {
            printf("[%s:%d] %s", err.filename.c_str(), err.linenum, err.lexeme.c_str());
        }

        return NULL_INDEX;
    }

    return program_root;
}

NodeIndex SageParser::parse_statements() {
    NodeIndex list_node = node_manager->create_block();

    while (errors.empty()) {
        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
        if (match_types(current_token->token_type, TT_EOF)) {
            break;
        }

        NodeIndex next_statement = parse_statement();
        if (next_statement == NULL_INDEX) {
            break; // don't want to add nullptr to children list if an error is found
        }

        node_manager->add_child(list_node, next_statement);
    }

    return list_node;
}

NodeIndex SageParser::parse_statement() {
    Token next_token;
    TokenType value_assign_slice[2] = {TT_ASSIGN, TT_FIELD_ACCESSOR};
    TokenType value_dec_slice[3] = {TT_KEYWORD, TT_LBRACKET, TT_IDENT};

    switch (current_token->token_type) {
        case TT_IDENT:
            next_token = peek();

            if (match_types(next_token.token_type, TT_BINDING)) {
                return parse_construct();
            }else if (matches_any(next_token.token_type, value_dec_slice, 3)) {
                return parse_value_dec();
            }else if (matches_any(next_token.token_type, value_assign_slice, 2)) {
                NodeIndex retval = parse_assign();
                if (retval == NULL_INDEX) {
                    // nil means that we actually were parsing an expression and should stop parsing for assign
                    break;
                }

                return retval;
            }else if (match_types(next_token.token_type, TT_LPAREN)) {
                return parse_function_call();
            }

            break;

        case TT_KEYWORD:
            return parse_keyword_statement();

        case TT_POUND:
            return parse_run_directive();

        default:
            break;
    }

    return parse_expression();
}

NodeIndex SageParser::parse_run_directive() {
    consume(TT_POUND, "Expected 'run' directive to begin with '#' symbol");

    if (current_token->token_type != TT_IDENT || current_token->lexeme != "run") {
        raise_error("Expected 'run' keyword in run directive statement.\n");
        return NULL_INDEX;
    }
    advance();

    Token run_token = Token(TT_COMPILER_CREATED, "#run { ... }", current_token->linenum);
    NodeIndex run_body = parse_body();

    return node_manager->create_unary(run_token, PN_RUN_DIRECTIVE, run_body);
}

NodeIndex SageParser::parse_value_dec() {
    Token name_identifier_token = Token();
    name_identifier_token.fill_with(*current_token);

    NodeIndex name_identifier_node = node_manager->create_unary(name_identifier_token, PN_IDENTIFIER);
    advance(); // move past identifier

    NodeIndex type_identifier_node = parse_type();
    if (type_identifier_node == NULL_INDEX) {
        raise_error("Found invalid type in value declaration list.\n");
        return NULL_INDEX;
    }

    Token type_identifier_token = node_manager->get_token(type_identifier_node);

    if (match_types(current_token->token_type, TT_ASSIGN)) {
        advance(); // move past the "=" symbol
        NodeIndex rhs = parse_expression();

        string new_lexeme = name_identifier_token.lexeme + " " + type_identifier_token.lexeme + " = " + node_manager->to_string(rhs);

        Token dec_token = Token(TT_ASSIGN, new_lexeme, name_identifier_token.linenum);
        return node_manager->create_trinary(dec_token, PN_ASSIGN, name_identifier_node, type_identifier_node, rhs);
    }

    string dec_lexeme = name_identifier_token.lexeme + " " + type_identifier_token.lexeme;
    
    // NOTE: probably could just make nodetype equal to ->get_nodetype() and don't need the this conditional
    ParseNodeType nodetype = PN_VAR_DEC;
    if (node_manager->get_nodetype(type_identifier_node) == PN_VARARG) {
        nodetype = PN_VARARG;
    }

    Token token = Token(TT_COMPILER_CREATED, dec_lexeme, name_identifier_token.linenum);
    return node_manager->create_binary(token, nodetype, name_identifier_node, type_identifier_node);
}

NodeIndex SageParser::parse_value_dec_list() {
    if (match_types(current_token->token_type, TT_RPAREN)) {
        Token token = Token(TT_COMPILER_CREATED, "empty parameter list", current_token->linenum);
        return node_manager->create_block(token, PN_PARAM_LIST);
    }

    while (match_types(current_token->token_type, TT_NEWLINE)) {
        advance();
    }

    if (!match_types(current_token->token_type, TT_IDENT)) {
        raise_error("Expecting value dec to begin with identifier.\n");
        return NULL_INDEX;
    }

    NodeIndex list_node = node_manager->create_block();
    BlockParseNode* _list_node = node_manager->unbox(list_node);
    _list_node->rep_nodetype = PN_PARAM_LIST;
    string list_token_lexeme = "";
    Token list_token = Token(TT_COMPILER_CREATED, "", current_token->linenum);

    while (match_types(current_token->token_type, TT_IDENT)) {
        NodeIndex value_dec = parse_value_dec();

        if (node_manager->get_host_nodetype(value_dec) == PN_TRINARY) {
            raise_error("Cannot initialize value in value declaration list.\n");
            return NULL_INDEX;
        }

        TokenType condition_slice[2] = {TT_RBRACE, TT_RPAREN};
        if (matches_any(current_token->token_type, condition_slice, 2)) {
            list_token.lexeme = node_manager->get_lexeme(value_dec);
            _list_node->children.push_back(value_dec);
            break;
        }

        consume(TT_COMMA, "Expected comma symbol after value declaration list.\n");
        list_token_lexeme += node_manager->get_lexeme(value_dec);
        _list_node->children.push_back(value_dec);

        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
    }

    list_token.lexeme = list_token_lexeme;
    _list_node->token = list_token;

    return list_node;
}

NodeIndex SageParser::parse_assign() {
    Token name_token = Token();
    name_token.fill_with(*current_token);

    NodeIndex name_node = node_manager->create_unary(name_token, PN_IDENTIFIER);
    
    Token lookahead = peek();
    bool is_field_access = match_types(lookahead.token_type, TT_FIELD_ACCESSOR);
    if (is_field_access) {
        node_manager->delete_node(name_node);

        name_node = parse_struct_field_access();
        name_token.lexeme = node_manager->get_full_lexeme(name_node);
    }else {
	// parse_struct_field_access leaves the current token cursor on the next token already
	// so if it is a field access then calling this p.advance() would be unnecessary
        advance(); // advance past the current identifier
    }

    // it could be that we are referencing the field in some expression
    if (is_field_access && current_token->token_type != TT_ASSIGN) {
        node_cache = name_node; // save parsed node for further expression parsing
        return NULL_INDEX;         // return null to signify that we actually are parsing a different kind of statement
    }

    consume(TT_ASSIGN, "Expected '=' symbol in assign statement.\n");

    NodeIndex value_node = parse_expression();

    string token_lexeme = name_token.lexeme + " = " + node_manager->get_lexeme(value_node);
    Token token = Token(TT_ASSIGN, token_lexeme, name_token.linenum);
    return node_manager->create_binary(token, PN_ASSIGN, name_node, value_node);
}

NodeIndex SageParser::parse_keyword_statement() {
    Token return_token = Token();

    unordered_map<string, int> lexeme_map = {
        {"ret", 1},
        {"if", 2},
        {"while", 3},
        {"for", 4}
    };

    switch (lexeme_map[current_token->lexeme]) {
    case 1:
        return_token.fill_with(*current_token);
        auto lookahead = peek();
        if (match_types(lookahead.token_type, TT_NEWLINE)) {
            advance();
            // return void
            return node_manager->create_unary(return_token, PN_KEYWORD);
        }
        
        advance(); // advance past return token
        NodeIndex expression = parse_expression();
        Token new_token = Token(TT_KEYWORD, "ret " + node_manager->to_string(expression), return_token.linenum);
        // return expression
        return node_manager->create_unary(new_token, PN_KEYWORD, expression);

    case 2: 
        return parse_if_statement();

    case 3:
        return parse_while_statement();

    case 4:
        return parse_for_statement();
    }

    raise_error("Could not recognize statement.\n");
    return NULL_INDEX;
}

NodeIndex SageParser::parse_if_statement() {
    advance(); // advance past the "if" keyword
    
    vector<NodeIndex> elif_statements = vector<NodeIndex>();

    NodeIndex condition_expression = parse_expression();
    Token expression_token = node_manager->get_token(condition_expression);

    NodeIndex condition_body = parse_body();

    Token prime_node_token = Token(TT_COMPILER_CREATED, "if ... { ... }", expression_token.linenum);
    NodeIndex primary_if = node_manager->create_binary(prime_node_token, PN_IF_BRANCH, condition_expression, condition_body);

    elif_statements.push_back(primary_if);

    while (current_token->lexeme == "else") {
        advance(); // move past else
        
        if (current_token->lexeme != "if" && current_token->lexeme != "{") {
            raise_error("Expected body or if statements after else keywords.\n");
            return NULL_INDEX;
        }

        if (current_token->lexeme == "if") {
            advance(); // move past if

            NodeIndex condition_exp = parse_expression();
            Token exp_token = node_manager->get_token(condition_exp);

            NodeIndex condition_body = parse_body();
            // TODO: more detailed token lexeme
            Token node_token = Token(TT_COMPILER_CREATED, "else if ... { ... }", exp_token.linenum);
            NodeIndex next_elif_statement = node_manager->create_binary(node_token, PN_IF_BRANCH, condition_exp, condition_body);
            elif_statements.push_back(next_elif_statement);

        } else if (match_types(current_token->token_type, TT_LBRACE)) {
            NodeIndex else_body = parse_body();
            Token body_token = node_manager->get_token(else_body);

            Token node_token = Token(TT_COMPILER_CREATED, "else { ... }", body_token.linenum);
            NodeIndex new_node = node_manager->create_unary(node_token, PN_ELSE_BRANCH, else_body);
            elif_statements.push_back(new_node);

            break;
        }
    }

    return node_manager->create_block(expression_token, PN_IF, elif_statements);
}

NodeIndex SageParser::parse_while_statement() {
    consume(TT_KEYWORD, "Expected 'while' keyword in while statement.\n");

    NodeIndex condition_node = parse_expression();
    auto node_token = node_manager->get_token(condition_node);

    NodeIndex body_node = parse_body();

    Token token = Token(TT_COMPILER_CREATED, "while <condition> { ... }", node_token.linenum);
    return node_manager->create_binary(token, PN_WHILE, condition_node, body_node);
}

NodeIndex SageParser::parse_for_statement() {
    consume(TT_KEYWORD, "Expected 'for' keyword in for statement.\n");

    if (match_types(current_token->token_type, TT_IDENT)) {
        raise_error("For statement expects iterator name.\n");
        return NULL_INDEX;
    }

    Token iterator_variable_token = Token();
    iterator_variable_token.fill_with(*current_token);
    NodeIndex identifier_token = node_manager->create_unary(iterator_variable_token, PN_IDENTIFIER);
    NodeIndex int_type_token = node_manager->create_unary(Token(TT_NUM, "i32", iterator_variable_token.linenum), PN_TYPE);
    NodeIndex iterator_variable_node = node_manager->create_binary(
        iterator_variable_token, 
        PN_VAR_DEC, 
        identifier_token, 
        int_type_token
    );
    advance(); // move past iterator
    
    if (current_token->lexeme != "in") {
        raise_error("Expected 'in' keyword in for statement.\n");
        return NULL_INDEX;
    }
    advance(); // move past in keyword
    
    NodeIndex range_node = parse_range();
    string range_lexeme = node_manager->get_lexeme(range_node);

    NodeIndex body_node = parse_body();

    string for_lexeme = "for " + iterator_variable_token.lexeme + " in " + range_lexeme + "{ ... }";
    Token for_token = Token(TT_COMPILER_CREATED, for_lexeme, iterator_variable_token.linenum);
    return node_manager->create_trinary(for_token, PN_FOR, iterator_variable_node, range_node, body_node);
}

NodeIndex SageParser::parse_range() {
    NodeIndex lhs = parse_expression();
    Token left_token = node_manager->get_token(lhs);

    consume(TT_RANGE, "Expected '...' operator in range statement.\n");

    NodeIndex rhs = parse_expression();

    string range_lex = left_token.lexeme + "..." + node_manager->get_lexeme(rhs);
    Token range_token = Token(TT_COMPILER_CREATED, range_lex, left_token.linenum);

    return node_manager->create_binary(range_token, PN_RANGE, lhs, rhs);
}

NodeIndex SageParser::parse_construct() {
    if (!match_types(current_token->token_type, TT_IDENT)) {
        raise_error("Expected identifier name at the beginning of construct statement.\n");
        return NULL_INDEX;
    }

    Token name_identifier_token = Token();
    name_identifier_token.fill_with(*current_token);
    NodeIndex name_identifier_node = node_manager->create_unary(name_identifier_token, PN_IDENTIFIER);
    advance(); // move identifier
    advance(); // move past the '::' symbol (we know its here because we peeked for it in parse_staetment()
    
    NodeIndex binding_node = NULL_INDEX;
    ParseNodeType nodetype;
    switch (current_token->token_type) {
    case TT_KEYWORD:
        // check if lexeme is 'struct'
        binding_node = parse_struct();
        nodetype = PN_STRUCT;
        break;

    case TT_LPAREN:
        // its a function
        binding_node = parse_function();
        nodetype = node_manager->get_nodetype(binding_node);
        break;

    default:
        binding_node = parse_type();
        nodetype = PN_TYPE;
        break;
    }

    if (binding_node == NULL_INDEX) {
        return NULL_INDEX;
    }

    string token_lexeme = name_identifier_token.lexeme + " :: " + node_manager->get_lexeme(binding_node);
    Token construct_token = Token(TT_COMPILER_CREATED, token_lexeme, name_identifier_token.linenum);
    return node_manager->create_binary(construct_token, nodetype, name_identifier_node, binding_node);
}

NodeIndex SageParser::parse_struct() {
    if (current_token->lexeme != "struct") {
        raise_error("Expected 'struct' keyword in structure definition.\n");
        return NULL_INDEX;
    }
    advance(); // advance past the struct keyword
    
    consume(TT_LBRACE, "Expected LBRACE in structure definition.\n");

    NodeIndex struct_contents = parse_value_dec_list();

    consume(TT_RBRACE, "Expected RBRACE in structure definition.\n");

    return node_manager->create_unary(node_manager->get_token(struct_contents), PN_STRUCT, struct_contents);
}

NodeIndex SageParser::parse_function() {
    consume(TT_LPAREN, "Expected LPAREN in function definition.\n");

    string signature_lexeme = "(";
    NodeIndex parameter_list = parse_value_dec_list();
    if (parameter_list == NULL_INDEX) {
        return NULL_INDEX;
    }
    Token parameter_token = node_manager->get_token(parameter_list);

    signature_lexeme += parameter_token.lexeme;

    consume(TT_RPAREN, "Expected RPAREN in function definition.\n");
    consume(TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition.\n");
    signature_lexeme += " ) -> ";

    if (!match_types(current_token->token_type, TT_KEYWORD)) {
        raise_error("Function must have a return type.\n");
        return NULL_INDEX;
    }

    Token return_type_token = Token();
    return_type_token.fill_with(*current_token);

    NodeIndex return_type_node = node_manager->create_unary(return_type_token, PN_TYPE);
    signature_lexeme += return_type_token.lexeme;
    advance(); // move past type keyword
    
    Token function_signature = Token(TT_COMPILER_CREATED, signature_lexeme, parameter_token.linenum);

    // if there is no brace and instead is immidiately followed by a NEWLINE then there is no body
    if (match_types(current_token->token_type, TT_NEWLINE)) {
        advance(); // move past the newline
        return node_manager->create_binary(function_signature, PN_FUNCDEC, parameter_list, return_type_node);
    }

    NodeIndex body_node = parse_body();
    return node_manager->create_trinary(function_signature, PN_FUNCDEF, parameter_list, return_type_node, body_node);
}

NodeIndex SageParser::parse_function_call() {
    Token function_name_token = Token();
    function_name_token.fill_with(*current_token);
    advance(); // move past func name identifier
    
    consume(TT_LPAREN, "Expected function call to include opening '(' bracket\n");

    Token params_token = Token(TT_COMPILER_CREATED, "", -1);
    NodeIndex params_node = node_manager->create_block(params_token, PN_BLOCK);

    while (true) {
        NodeIndex param = parse_primary();
        node_manager->add_child(params_node, param);

        if (!match_types(current_token->token_type, TT_COMMA)) {
            break;
        }

        advance();
    }

    consume(TT_RPAREN, "Expected function call to include closing ')' bracket.\n");

    return node_manager->create_unary(function_name_token, PN_FUNCCALL, params_node);
}

NodeIndex SageParser::parse_body() {
    consume(TT_LBRACE, "Expected LBRACE in body definition statement\n");

    Token body_token = Token(TT_COMPILER_CREATED, "{ ... }", current_token->linenum);
    NodeIndex body_node = node_manager->create_block(body_token, PN_BLOCK);
    // body_node->token = body_token;

    while (true) {
        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
        if (match_types(current_token->token_type, TT_RBRACE)) {
            break;
        }

        NodeIndex next_statement = parse_statement();
        if (next_statement == NULL_INDEX) {
            break;
        }

        node_manager->add_child(body_node, next_statement);
    }

    consume(TT_RBRACE, "Expected RBRACE in body definition statement.\n");

    // body_node->host_nodetype = PN_BLOCK;
    return body_node;
}

NodeIndex SageParser::parse_type() {
    NodeIndex return_node = NULL_INDEX;

    if (match_types(current_token->token_type, TT_LPAREN)) {
        NodeIndex function_type = parse_function();

        return_node = node_manager->create_unary(node_manager->get_token(function_type), PN_TYPE, function_type);

        return return_node;
    }else if (match_types(current_token->token_type, TT_LBRACKET)) {
        // keep track of how many dims this array has
        int lbracket_nest_count = 0;
        string token_lexeme = "";
        while (match_types(current_token->token_type, TT_LBRACKET)) {
            advance(); // move past lbracket
            lbracket_nest_count++;
            token_lexeme += "[";
        }

        TokenType slice[2] = {TT_KEYWORD, TT_IDENT};
        if (!matches_any(current_token->token_type, slice, 2)) {
            raise_error("Expected valid type identifier in array type.\n");
            return NULL_INDEX;
        }

        Token array_type_token = Token();
        array_type_token.fill_with(*current_token);
        NodeIndex array_type_node = node_manager->create_unary(array_type_token, PN_TYPE);
        token_lexeme += array_type_token.lexeme;
        advance(); // advance past the identifier
        
        if (match_types(current_token->token_type, TT_COLON)) {
            advance(); // advance past the colon 
            
            if (!match_types(current_token->token_type, TT_NUM)) {
                raise_error("Expected a number to define the length of the array type declaration.\n");
                node_manager->delete_node(array_type_node);
                return NULL_INDEX;
            }

            advance(); // advance past the number
        }

        while (match_types(current_token->token_type, TT_RBRACKET)) {
            advance();
            lbracket_nest_count--;
            token_lexeme += "]";
        }

        if (lbracket_nest_count != 0) {
            raise_error("Unambiguous array nesting found, please ensure all '[' have a matching ']'.\n");
            return NULL_INDEX;
        }

        Token new_token = Token(array_type_token.token_type, token_lexeme, array_type_token.linenum);
        return_node = node_manager->create_unary(new_token, PN_TYPE, array_type_node);
    }else if (match_types(current_token->token_type, TT_VARARG)) {
        advance(); // advance past the vararg '...' notation
        Token type_token = Token();
        type_token.fill_with(*current_token);
        return_node = node_manager->create_unary(type_token, PN_VARARG);
        advance();
    } else {
        Token type_token = Token();
        type_token.fill_with(*current_token);
        return_node = node_manager->create_unary(type_token, PN_TYPE);
        advance(); // advance past the identifier
    }

    if (current_token->lexeme == "*") {
        advance(); // advance past the star
        Token retnode_token = node_manager->get_token(return_node);
        Token new_token = Token(retnode_token.token_type, retnode_token.lexeme + "*", retnode_token.linenum);
        NodeIndex new_return_node = node_manager->create_unary(new_token, PN_TYPE, return_node);

        return new_return_node;
    }

    return return_node;
}

NodeIndex SageParser::parse_expression() {
    // if the node cache isn't empty then that means we were previously parsing,
    //	  an identifier that looked like another statement but actually was an expression,
    //	  so instead of parsing for a primary we simply use the primary that was already found first.
    if (node_cache != NULL_INDEX) {
        NodeIndex first_primary = node_cache;
        node_cache = NULL_INDEX;

        return parse_operator(first_primary, TT_EQUALITY);
    }
    return parse_operator(parse_primary(), TT_EQUALITY);
}

bool is_operator(Token op) { 
    // TT_DIV should be 8 which is the last operator so far
    return op.token_type <= 8; 
}

int decide_precedence_inc(TokenType curr_op_type, TokenType op_type) { 
    if (curr_op_type > op_type) {
        return 1;
    }
    return 0;
}

NodeIndex SageParser::parse_operator(NodeIndex left, int min_precedence) {
    // this should be an operator token because LHS will have already been evaluated
    Token current_op = Token(); 
    current_op.fill_with(*current_token);

    while (is_operator(current_op) && current_op.get_operator_precedence() >= min_precedence) {
        Token op = current_op;
        advance();

        NodeIndex right = parse_primary();
        current_op.fill_with(*current_token);

        while (
            (is_operator(current_op) && current_op.get_operator_precedence() > op.get_operator_precedence()) || 
            (op_is_right_associative(current_op.lexeme) && current_op.get_operator_precedence() == op.get_operator_precedence())
        ) {
            int inc = decide_precedence_inc(current_op.token_type, op.token_type);
            right = parse_operator(right, op.get_operator_precedence() + inc);

            current_op.fill_with(*current_token);
        }
        left = node_manager->create_binary(op, PN_BINARY, left, right);
    }
    return left;
}

NodeIndex SageParser::parse_primary() {
    Token token = Token();
    Token lookahead;
    NodeIndex ret = NULL_INDEX;
    NodeIndex expression = NULL_INDEX;
    switch (current_token->token_type) {
        case TT_NUM:
            token.fill_with(*current_token);
            ret = node_manager->create_unary(token, PN_NUMBER);
            advance();
            return ret;

        case TT_FLOAT:
            token.fill_with(*current_token);
            ret = node_manager->create_unary(token, PN_FLOAT);
            advance();
            return ret;

        case TT_IDENT:
            token.fill_with(*current_token);
            lookahead = peek();
            if (match_types(lookahead.token_type, TT_FIELD_ACCESSOR)) {
                return parse_struct_field_access();
            }else if (match_types(lookahead.token_type, TT_LPAREN)) {
                return parse_function_call();
            }

            ret = node_manager->create_unary(token, PN_VAR_REF);
            advance();
            return ret;

        case TT_LPAREN:
            consume(TT_LPAREN, "Expected opening '('");
            expression = parse_expression();
            consume(TT_RPAREN, "Expected closing ')'");
            return expression;

        case TT_STRING:
            token.fill_with(*current_token);
            ret = node_manager->create_unary(token, PN_STRING);
            advance();
            return ret;
        
        default:
            printf("current tok : %s\n", current_token->lexeme.c_str());
            raise_error("Unrecognized statement; could not find valid primary.\n");
            return NULL_INDEX;
    }
}

NodeIndex SageParser::parse_struct_field_access() {
    vector<string> identifier_lexemes = vector<string>();
    identifier_lexemes.push_back(current_token->lexeme);
    advance();

    while (match_types(current_token->token_type, TT_FIELD_ACCESSOR)) {
        advance();
        if (!match_types(current_token->token_type, TT_IDENT)) {
            raise_error("Expected identifier in struct field accessor statement.\n");
            return NULL_INDEX;
        }

        identifier_lexemes.push_back(current_token->lexeme);
        advance();
    }

    Token token = Token(TT_COMPILER_CREATED, identifier_lexemes[0], -1);
    return node_manager->create_unary(token, PN_LIST, identifier_lexemes);
}

// parser utility methods
void SageParser::raise_error(string message) {
    Token error_token = Token(message, current_token->linenum);
    error_token.filename = filename;
    errors.push_back(error_token);
}

bool SageParser::match_types(TokenType type_a, TokenType type_b) {
    return type_a == type_b;
}

bool SageParser::matches_any(TokenType type_a, TokenType* possible_types, int type_amount) {
    for (int i = 0; i < type_amount; ++i) {
        if (match_types(type_a, possible_types[i])) {
            return true;
        }
    }
    return false;
}


// if the current token is token_type then advance otherwise throw an error
// used to expected certain tokens in the input
void SageParser::consume(TokenType tokentype, string message) {
    if (current_token->token_type == TT_EOF) {
        raise_error(message);
        return;
    }

    if (current_token->token_type == tokentype) {
        advance();
        return;
    }

    raise_error(message);
}

void SageParser::advance() {
    current_token = lexer->get_token();
}

Token SageParser::peek() {
    // need to save the current token so that it doesn't get lost when its replaced by lexer->get_token()
    Token current_stash = Token();
    current_stash.fill_with(*current_token);

    auto retval = lexer->get_token();

    Token peeked_token = Token();
    peeked_token.fill_with(*retval);

    // once done peeking the next token we can reinstate the current token
    current_token->fill_with(current_stash);

    lexer->unget_token();
    return peeked_token;
}

bool SageParser::op_is_left_associative(string op_literal) {
    unordered_map<string, bool> left_map = {
        {"*", true},
        {"/", true},
        {"-", true},
        {"+", true},
        {"==", true},
    };

    return left_map[op_literal];
}

bool SageParser::op_is_right_associative(string op_literal) {
    return !op_is_left_associative(op_literal);
}
