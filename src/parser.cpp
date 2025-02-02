#include "../include/lexer.h"
#include "../include/parse_node.h"
#include "../include/token.h"
#include "../include/parser.h"

#include <unordered_map>
#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <string>
using namespace std;

int decide_precedence_inc(TokenType curr_op_type, TokenType op_type);
bool is_operator(TokenType tokentype);

Parser::Parser(string filename) {
    lexer = new Lexer(filename);

    current_token = nullptr;
    node_cache = nullptr;
    errors = vector<Token>();
}

AbstractParseNode* Parser::parse_program(bool debug_lexer) {
    if (debug_lexer) {
        Token* walk_token = current_token;
        int counter = 0;
        while (walk_token->token_type != TT_EOF) {
            current_token = lexer->get_token();
            printf("[%d] %s\n", counter, walk_token->to_string().c_str());
        }

        return nullptr;
    }

    current_token = lexer->get_token();
    BlockParseNode* root = parse_libraries();
    BlockParseNode* program_root = parse_statements();

    // add all of program_root's children to root's children
    root->children.insert(root->children.end(), program_root->children.begin(), program_root->children.end());

    // clear program_root's children to avoid unused duplicate pointers
    program_root->children.clear();
    
    if (errors.size() != 0) {
        for (Token err : errors) {
            printf("%s\n", err.to_string().c_str());
        }
        for (auto child : root->children) {
            delete child;
        }
        return nullptr;
    }

    return root;
}

AbstractParseNode* Parser::library_statement() {
    consume(TT_KEYWORD, "Expected include keyword in include statement.\n");

    if (current_token->token_type != TT_STRING) {
        raise_error("Expected string literal in include statement.\n");
        return nullptr;
    }

    Token string_token = Token();
    string_token.fill_with(*current_token);

    UnaryParseNode* node = new UnaryParseNode(string_token, PN_INCLUDE);

    advance();
    consume(TT_NEWLINE, "Must have include statements on individual lines.\n");

    return node;
}

BlockParseNode* Parser::parse_libraries() {
    BlockParseNode* list_node = new BlockParseNode();

    while (errors.empty()) {
        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }

        if (!match_types(current_token->token_type, TT_KEYWORD)) {
            break;
        }

        auto next_include = library_statement();
        list_node->children.push_back(next_include);
    }

    return list_node;
}

BlockParseNode* Parser::parse_statements() {
    BlockParseNode* list_node = new BlockParseNode();

    while (errors.empty()) {
        printf("iterating\n");
        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
        if (match_types(current_token->token_type, TT_EOF)) {
            break;
        }
        auto next_statement = parse_statement();
        list_node->children.push_back(next_statement);
    }

    return list_node;
}

AbstractParseNode* Parser::parse_statement() {
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
                auto retval = parse_assign();
                if (retval == nullptr) {
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

AbstractParseNode* Parser::parse_run_directive() {
    consume(TT_POUND, "Expected 'run' directive to begin with '#' symbol");

    if (current_token->token_type != TT_IDENT || current_token->lexeme == "run") {
        raise_error("Expected 'run' keyword in compile time execution statement.\n");
        return nullptr;
    }
    advance();

    Token run_token = Token(TT_COMPILER_CREATED, "#run { ... }", current_token->linenum);
    auto run_body = parse_body();

    return new UnaryParseNode(run_token, PN_RUN_DIRECTIVE, run_body);
}

AbstractParseNode* Parser::parse_value_dec() {
    Token name_identifier_token = Token();
    name_identifier_token.fill_with(*current_token);

    UnaryParseNode* name_identifier_node = new UnaryParseNode(name_identifier_token, PN_IDENTIFIER);
    advance(); // move past identifier

    AbstractParseNode* type_identifier_node = parse_type();
    if (type_identifier_node == nullptr) {
        raise_error("Found invalid type in value declaration list.\n");
        delete name_identifier_node;
        return nullptr;
    }

    Token type_identifier_token = type_identifier_node->get_token();

    if (match_types(current_token->token_type, TT_ASSIGN)) {
        advance(); // move past the "=" symbol
        auto rhs = parse_expression();

        string new_lexeme = name_identifier_token.lexeme + " " + type_identifier_token.lexeme + " = " + rhs->to_string();

        Token dec_token = Token(TT_ASSIGN, new_lexeme, name_identifier_token.linenum);
        return new TrinaryParseNode(dec_token, PN_ASSIGN, name_identifier_node, type_identifier_node, rhs);
    }

    string dec_lexeme = name_identifier_token.lexeme + " " + type_identifier_token.lexeme;
    
    // NOTE: probably could just make nodetype equal to ->get_nodetype() and don't need the this conditional
    ParseNodeType nodetype = PN_VAR_DEC;
    if (type_identifier_node->get_nodetype() == PN_VARARG) {
        nodetype = PN_VARARG;
    }

    Token token = Token(TT_COMPILER_CREATED, dec_lexeme, name_identifier_token.linenum);
    return new BinaryParseNode(token, nodetype, name_identifier_node, type_identifier_node);
}

AbstractParseNode* Parser::parse_value_dec_list() {
    if (match_types(current_token->token_type, TT_RPAREN)) {
        Token token = Token(TT_COMPILER_CREATED, "empty parameter list", current_token->linenum);
        return new BlockParseNode(token, PN_PARAM_LIST);
    }

    while (match_types(current_token->token_type, TT_NEWLINE)) {
        advance();
    }

    if (!match_types(current_token->token_type, TT_IDENT)) {
        raise_error("Expecting value dec to begin with identifier.\n");
        return nullptr;
    }

    BlockParseNode* list_node = new BlockParseNode();
    list_node->rep_nodetype = PN_PARAM_LIST;
    string list_token_lexeme = "";
    Token list_token = Token(TT_COMPILER_CREATED, "", current_token->linenum);

    while (match_types(current_token->token_type, TT_IDENT)) {
        auto value_dec = parse_value_dec();

        if (value_dec->get_host_nodetype() == PN_TRINARY) {
            raise_error("Cannot initialize value in value declaration list.\n");
            return nullptr;
        }

        TokenType condition_slice[2] = {TT_RBRACE, TT_RPAREN};
        if (matches_any(current_token->token_type, condition_slice, 2)) {
            list_token.lexeme = value_dec->get_token().lexeme;
            list_node->children.push_back(value_dec);
            break;
        }

        consume(TT_COMMA, "Expected comma symbol after value declaration list.\n");
        list_token_lexeme += value_dec->get_token().lexeme;
        list_node->children.push_back(value_dec);

        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
    }

    list_token.lexeme = list_token_lexeme;
    list_node->token = list_token;

    return list_node;
}

AbstractParseNode* Parser::parse_assign() {
    Token name_token = Token();
    name_token.fill_with(*current_token);

    auto name_node = new UnaryParseNode(name_token, PN_IDENTIFIER);
    
    Token lookahead = peek();
    bool is_field_access = match_types(lookahead.token_type, TT_FIELD_ACCESSOR);
    if (is_field_access) {
        auto name_node = parse_struct_field_access();
        name_token.lexeme = name_node->get_full_lexeme();
    }else {
		// parse_struct_field_access leaves the current token cursor on the next token already
		// so if it is a field access then calling this p.advance() would be unnecessary
        advance(); // advance past the current identifier
    }

    // it could be that we are referencing the field in some expression
    if (is_field_access && current_token->token_type != TT_ASSIGN) {
        node_cache = name_node; // save parsed node for further expression parsing
        return nullptr;         // return null to signify that we actually are parsing a different kind of statement
    }

    consume(TT_ASSIGN, "Expected '=' symbol in assign statement.\n");

    auto value_node = parse_expression();

    string token_lexeme = name_token.lexeme + " = " + value_node->get_token().lexeme;
    Token token = Token(TT_ASSIGN, token_lexeme, name_token.linenum);
    return new BinaryParseNode(token, PN_ASSIGN, name_node, value_node);
}

AbstractParseNode* Parser::parse_keyword_statement() {
    Token return_token = Token();

    unordered_map<string, int> lexeme_map = {
        {"ret", 1},
        {"if", 2},
        {"while", 3},
        {"for", 4}
    };

    switch (lexeme_map[current_token->lexeme]) {
    case 1: {
        return_token.fill_with(*current_token);
        auto lookahead = peek();
        if (match_types(lookahead.token_type, TT_NEWLINE)) {
            advance();
            return new UnaryParseNode(return_token, PN_KEYWORD);
        }
        
        advance(); // advance past return token
        auto expression = parse_expression();
        Token new_token = Token(TT_KEYWORD, "ret " + expression->to_string(), return_token.linenum);
        return new UnaryParseNode(new_token, PN_KEYWORD, expression);
    }

    case 2: 
        return parse_if_statement();

    case 3:
        return parse_while_statement();

    case 4:
        return parse_for_statement();
    } 

    raise_error("Could not recognize statement.\n");
    return nullptr;
}

AbstractParseNode* Parser::parse_if_statement() {
    advance(); // advance past the "if" keyword
    
    vector<AbstractParseNode*> elif_statements = vector<AbstractParseNode*>();

    auto condition_expression = parse_expression();
    auto condition_body = parse_body();
    Token prime_node_token = Token(TT_COMPILER_CREATED, "if ... { ... }", condition_expression->get_token().linenum);
    BinaryParseNode* primary_if = new BinaryParseNode(prime_node_token, PN_IF_BRANCH, condition_expression, condition_body);

    elif_statements.push_back(primary_if);

    while (current_token->lexeme == "else") {
        advance(); // move past else
        
        if (current_token->lexeme != "if" && current_token->lexeme != "{") {
            raise_error("Expected body or if statements after else keywords.\n");
            return nullptr;
        }

        if (current_token->lexeme == "if") {
            advance(); // move past if

            auto condition_exp = parse_expression();
            auto condition_body = parse_body();
            // TODO: more detailed token lexeme
            Token node_token = Token(TT_COMPILER_CREATED, "else if ... { ... }", condition_exp->get_token().linenum);
            BinaryParseNode* next_elif_statement = new BinaryParseNode(node_token, PN_IF_BRANCH, condition_exp, condition_body);
            elif_statements.push_back(next_elif_statement);

        } else if (match_types(current_token->token_type, TT_LBRACE)) {
            auto else_body = parse_body();
            Token node_token = Token(TT_COMPILER_CREATED, "else { ... }", else_body->get_token().linenum);
            UnaryParseNode* new_node = new UnaryParseNode(node_token, PN_ELSE_BRANCH, else_body);
            elif_statements.push_back(new_node);

            break;
        }
    }

    return new BlockParseNode(condition_expression->get_token(), PN_IF, elif_statements);
}

AbstractParseNode* Parser::parse_while_statement() {
    consume(TT_KEYWORD, "Expected 'while' keyword in while statement.\n");

    auto condition_node = parse_expression();

    auto body_node = parse_body();

    Token token = Token(TT_COMPILER_CREATED, "while <condition> { ... }", condition_node->get_token().linenum);
    return new BinaryParseNode(token, PN_WHILE, condition_node, body_node);
}

AbstractParseNode* Parser::parse_for_statement() {
    consume(TT_KEYWORD, "Expected 'for' keyword in for statement.\n");

    if (match_types(current_token->token_type, TT_IDENT)) {
        raise_error("For statement expects iterator name.\n");
        return nullptr;
    }

    Token iterator_variable_token = Token();
    iterator_variable_token.fill_with(*current_token);
    auto iterator_variable_node = new UnaryParseNode(iterator_variable_token, PN_VAR_DEC);
    advance(); // move past iterator
    
    if (current_token->lexeme != "in") {
        raise_error("Expected 'in' keyword in for statement.\n");
        return nullptr;
    }
    advance(); // move past in keyword
    
    auto range_node = parse_range();
    auto body_node = parse_body();

    string for_lexeme = "for " + iterator_variable_token.lexeme + " in " + range_node->get_token().lexeme + "{ ... }";
    Token for_token = Token(TT_COMPILER_CREATED, for_lexeme, iterator_variable_token.linenum);
    return new TrinaryParseNode(for_token, PN_FOR, iterator_variable_node, range_node, body_node);
}

AbstractParseNode* Parser::parse_range() {
    auto lhs = parse_expression();

    consume(TT_RANGE, "Expected '...' operator in range statement.\n");

    auto rhs = parse_expression();

    string range_lex = lhs->get_token().lexeme + "..." + rhs->get_token().lexeme;
    Token range_token = Token(TT_COMPILER_CREATED, range_lex, lhs->get_token().linenum);

    return new BinaryParseNode(range_token, PN_RANGE, lhs, rhs);
}

AbstractParseNode* Parser::parse_construct() {
    if (!match_types(current_token->token_type, TT_IDENT)) {
        raise_error("Expected identifier name at the beginning of struct statement.\n");
        return nullptr;
    }

    Token name_identifier_token = Token();
    name_identifier_token.fill_with(*current_token);
    auto name_identifier_node = new UnaryParseNode(name_identifier_token, PN_IDENTIFIER);
    advance(); // move identifier
    
    AbstractParseNode* binding_node;
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
        nodetype = binding_node->get_nodetype();
        break;

    default:
        binding_node = parse_type();
        nodetype = PN_TYPE;
        break;
    }

    if (binding_node == nullptr) {
        return nullptr;
    }

    string token_lexeme = name_identifier_token.lexeme + " :: " + binding_node->get_token().lexeme;
    Token construct_token = Token(TT_COMPILER_CREATED, token_lexeme, name_identifier_token.linenum);
    return new BinaryParseNode(construct_token, nodetype, name_identifier_node, binding_node);
}

AbstractParseNode* Parser::parse_struct() {
    if (current_token->lexeme != "struct") {
        raise_error("Expected 'struct' keyword in structure definition.\n");
        return nullptr;
    }
    advance(); // advance past the struct keyword
    
    consume(TT_LBRACE, "Expected LBRACE in structure definition.\n");

    auto struct_contents = parse_value_dec_list();

    consume(TT_RBRACE, "Expected RBRACE in structure definition.\n");

    return new UnaryParseNode(struct_contents->get_token(), PN_STRUCT, struct_contents);
}

AbstractParseNode* Parser::parse_function() {
    consume(TT_LPAREN, "Expected LPAREN in function definition.\n");

    string signature_lexeme = "(";
    auto parameter_list = parse_value_dec_list();
    if (parameter_list == nullptr) {
        return nullptr;
    }

    signature_lexeme += parameter_list->get_token().lexeme;

    consume(TT_RPAREN, "Expected RPAREN in function definition.\n");
    consume(TT_FUNC_RETURN_TYPE, "Expected '->' symbol in function definition.\n");
    signature_lexeme += " ) -> ";

    if (!match_types(current_token->token_type, TT_KEYWORD)) {
        raise_error("Function must have a return type.\n");
        return nullptr;
    }

    Token return_type_token = Token();
    return_type_token.fill_with(*current_token);

    auto return_type_node = new UnaryParseNode(return_type_token, PN_TYPE);
    signature_lexeme += return_type_token.lexeme;
    advance(); // move past type keyword
    
    Token function_signature = Token(TT_COMPILER_CREATED, signature_lexeme, parameter_list->get_token().linenum);

    // if there is no brace and instead if immidiately followed by a NEWLINE then there is no body
    if (match_types(current_token->token_type, TT_NEWLINE)) {
        advance(); // move past the newline
        return new BinaryParseNode(function_signature, PN_FUNCDEC, parameter_list, return_type_node);
    }

    auto body_node = parse_body();
    return new TrinaryParseNode(function_signature, PN_FUNCDEF, parameter_list, return_type_node, body_node);
}

AbstractParseNode* Parser::parse_function_call() {
    Token function_name_token = Token();
    function_name_token.fill_with(*current_token);
    advance(); // move past func name identifier
    
    consume(TT_LPAREN, "Expected function call to include opening '(' bracket\n");

    vector<AbstractParseNode*> params = vector<AbstractParseNode*>();
    while (true) {
        auto param = parse_primary();
        params.push_back(param);

        if (!match_types(current_token->token_type, TT_COMMA)) {
            break;
        }

        advance();
    }

    consume(TT_RPAREN, "Expected function call to include closing ')' bracket.\n");

    Token params_token = Token(TT_COMPILER_CREATED, "", -1);
    auto params_node = new BlockParseNode(params_token, PN_BLOCK, params);

    return new UnaryParseNode(function_name_token, PN_FUNCCALL, params_node);
}

AbstractParseNode* Parser::parse_body() {
    consume(TT_LBRACE, "Expected LBRACE in body definition statement\n");

    Token body_token = Token(TT_COMPILER_CREATED, "{ ... }", current_token->linenum);
    auto body_node = new BlockParseNode(body_token, PN_BLOCK);
    // body_node->token = body_token;

    while (true) {
        while (match_types(current_token->token_type, TT_NEWLINE)) {
            advance();
        }
        if (match_types(current_token->token_type, TT_RBRACE)) {
            break;
        }

        auto next_statement = parse_statement();
        body_node->children.push_back(next_statement);
    }

    consume(TT_RBRACE, "Expected RBRACE in body definition statement.\n");

    // body_node->host_nodetype = PN_BLOCK;
    return body_node;
}

AbstractParseNode* Parser::parse_type() {
    UnaryParseNode* return_node;

    if (match_types(current_token->token_type, TT_LPAREN)) {
        auto function_type = parse_function();
        return_node = new UnaryParseNode(function_type->get_token(), PN_TYPE, function_type);

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
            return nullptr;
        }

        Token array_type_token = Token();
        array_type_token.fill_with(*current_token);
        auto array_type_node = new UnaryParseNode(array_type_token, PN_TYPE);
        token_lexeme += array_type_token.lexeme;
        advance(); // advance past the identifier
        
        if (match_types(current_token->token_type, TT_COLON)) {
            advance(); // advance past the colon 
            
            if (!match_types(current_token->token_type, TT_NUM)) {
                raise_error("Expected a number to define the length of the array type declaration.\n");
                delete array_type_node;
                return nullptr;
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
            return nullptr;
        }

        Token new_token = Token(array_type_token.token_type, token_lexeme, array_type_token.linenum);
        return_node = new UnaryParseNode(new_token, PN_TYPE, array_type_node);
    }else if (match_types(current_token->token_type, TT_VARARG)) {
        advance(); // advance past the vararg '...' notation
        Token type_token = Token();
        type_token.fill_with(*current_token);
        return_node = new UnaryParseNode(type_token, PN_VARARG);
        advance();
    } else {
        Token type_token = Token();
        type_token.fill_with(*current_token);
        return_node = new UnaryParseNode(type_token, PN_TYPE);
        advance(); // advance past the identifier
    }

    if (current_token->lexeme == "*") {
        advance(); // advance past the star
        Token retnode_token = return_node->get_token();
        Token new_token = Token(retnode_token.token_type, retnode_token.lexeme + "*", retnode_token.linenum);
        auto new_return_node = new UnaryParseNode(new_token, PN_TYPE, return_node);

        return new_return_node;
    }

    return return_node;
}

AbstractParseNode* Parser::parse_expression() {
    // if the node cache isn't empty then that means we were previously parsing,
	//	  an identifier that looked like another statement but actually was an expression,
	//	  so instead of parsing for a primary we simply use the primary that was already found first.
    if (node_cache != nullptr) {
        auto first_primary = node_cache;
        node_cache = nullptr;

        return parse_operator(first_primary, TT_EQUALITY);
    }
    return parse_operator(parse_primary(), TT_EQUALITY);
}

AbstractParseNode* Parser::parse_operator(AbstractParseNode* left, int min_precedence) {
    Token current = Token();
    current.fill_with(*current_token);
    while (is_operator(current_token->token_type) && current.get_operator_precedence() >= min_precedence) {
        Token op = current;
        advance();

        auto right = parse_primary();
        current.fill_with(*current_token);
        while (is_operator(current_token->token_type) && (current_token->token_type > op.token_type ||
               (op_is_right_associative(current.lexeme) && current.token_type == op.token_type))) {

            int inc = decide_precedence_inc(current.token_type, op.token_type);
            right = parse_operator(right, op.token_type+inc);

            current.fill_with(*current_token);
        }
        left = new BinaryParseNode(op, PN_BINARY, left, right);
    }

    return left;
}

AbstractParseNode* Parser::parse_primary() {
    Token token = Token();
    Token lookahead;
    AbstractParseNode* ret;
    AbstractParseNode* expression;
    switch (current_token->token_type) {
        case TT_NUM:
            token.fill_with(*current_token);
            ret = new UnaryParseNode(token, PN_NUMBER);
            advance();
            return ret;

        case TT_FLOAT:
            token.fill_with(*current_token);
            ret = new UnaryParseNode(token, PN_FLOAT);
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

            ret = new UnaryParseNode(token, PN_VAR_REF);
            advance();
            return ret;

        case TT_LPAREN:
            consume(TT_LPAREN, "Expected opening '('");
            expression = parse_expression();
            consume(TT_RPAREN, "Expected closing ')'");
            return expression;

        case TT_STRING:
            token.fill_with(*current_token);
            ret = new UnaryParseNode(token, PN_STRING);
            advance();
            return ret;
        
        default:
            raise_error("Unrecognized statement; could not find valid primary.\n");
            return nullptr;
    }
}

UnaryParseNode* Parser::parse_struct_field_access() {
    vector<string> identifier_lexemes = vector<string>();
    identifier_lexemes.push_back(current_token->lexeme);
    advance();

    while (match_types(current_token->token_type, TT_FIELD_ACCESSOR)) {
        advance();
        if (!match_types(current_token->token_type, TT_IDENT)) {
            raise_error("Expected identifier in struct field accessor statement.\n");
            return nullptr;
        }

        identifier_lexemes.push_back(current_token->lexeme);
        advance();
    }

    Token token = Token(TT_COMPILER_CREATED, identifier_lexemes[0], -1);
    return new UnaryParseNode(token, PN_LIST, identifier_lexemes);
}

bool is_operator(TokenType tokentype) {
    return tokentype <= 9; // TT_EXP should be 9 which is the last operator so far
}

int decide_precedence_inc(TokenType curr_op_type, TokenType op_type) {
    if (curr_op_type > op_type) {
        return 1;
    } 
    return 0;
}

// parser utility methods
Token Parser::raise_error(string message) {
    Token error_token = Token(message, current_token->linenum);
    errors.push_back(error_token);
    return error_token;
}

bool Parser::match_types(TokenType type_a, TokenType type_b) {
    return type_a == type_b;
}

bool Parser::matches_any(TokenType type_a, TokenType* possible_types, int type_amount) {
    for (int i = 0; i < type_amount; ++i) {
        if (match_types(type_a, possible_types[i])) {
            return true;
        }
    }
    return false;
}


// if the current token is token_type then advance otherwise throw an error
// used to expected certain tokens in the input
Token Parser::consume(TokenType tokentype, string message) {
    if (current_token_type_is(tokentype)) {
        advance();
        return *current_token;
    }

    Token error = Token(message, current_token->linenum);
    return error;
}

void Parser::advance() {
    current_token = lexer->get_token();
}

Token Parser::peek() {
    auto retval = lexer->get_token();
    lexer->unget_token();
    return *retval;
}

bool Parser::current_token_type_is(TokenType tokentype) {
    if (is_ending_token()) {
        return false;
    }

    return current_token->token_type == tokentype;
}

bool Parser::is_ending_token() {
    return current_token->token_type == TT_EOF;
}

bool Parser::op_is_left_associative(string op_literal) {
    unordered_map<string, bool> left_map = {
        {"*", true},
        {"/", true},
        {"-", true},
        {"+", true},
        {"==", true},
        {"^", true},
    };

    return left_map[op_literal];
}

bool Parser::op_is_right_associative(string op_literal) {
    return !op_is_left_associative(op_literal);
}
