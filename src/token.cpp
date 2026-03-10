#include <string>
#include <cstdlib>
#include <cstdio>
#include "../include/token.h"

Token::Token(TokenType type, std::string lexeme, int linenum) {
    this->lexeme = lexeme;
    this->token_type = type;
    this->linenum = linenum;
}

Token::Token(std::string err_message, int linenum) {
    this->token_type = TT_ERROR;
    this->lexeme = err_message;
    this->linenum = linenum;
}

Token::Token() {
    this->lexeme = "";
    this->filename = "";
    this->token_type = TT_EOF;
    this->linenum = -1;
    this->linedepth = -1;
}

void Token::fill_with(Token copy_token) {
    this->lexeme = copy_token.lexeme;
    this->filename = copy_token.filename;
    this->token_type = copy_token.token_type;
    this->linenum = copy_token.linenum;
    this->linedepth = copy_token.linedepth;
}

string Token::to_string() {
    switch (token_type) {
        case TT_EQUALITY:
            return "TT_EQUALITY";
        case TT_LT:
            return "TT_LT";
        case TT_GT:
            return "TT_GT";
        case TT_GTE:
            return "TT_GTE";
        case TT_LTE:
            return "TT_LTE";
        case TT_ADD:
            return "TT_ADD";
        case TT_SUB:
            return "TT_SUB";
        case TT_STAR:
            return "TT_STAR";
        case TT_DIV:
            return "TT_DIV";
        case TT_NUM:
            return "TT_NUM";
        case TT_IDENT:
            return "TT_IDENT";
        case TT_FLOAT:
            return "TT_FLOAT";
        case TT_KEYWORD:
            return "TT_KEYWORD";
        case TT_NEWLINE:
            return "TT_NEWLINE";
        case TT_ASSIGN:
            return "TT_ASSIGN";
        case TT_LPAREN:
            return "TT_LPAREN";
        case TT_RPAREN:
            return "TT_RPAREN";
        case TT_LBRACE:
            return "TT_LBRACE";
        case TT_RBRACE:
            return "TT_RBRACE";
        case TT_LBRACKET:
            return "TT_LBRACKET";
        case TT_RBRACKET:
            return "TT_RBRACKET";
        case TT_COMMA:
            return "TT_COMMA";
        case TT_FUNC_RETURN_TYPE:
            return "TT_FUNC_RETURN_TYPE";
        case TT_INCLUDE:
            return "TT_INCLUDE";
        case TT_STRING:
            return "TT_STRING";
        case TT_EOF:
            return "TT_EOF";
        case TT_SPACE:
            return "TT_SPACE";
        case TT_ERROR:
            return "TT_ERROR";
        case TT_BINDING:
            return "TT_BINDING";
        case TT_RANGE:
            return "TT_RANGE";
        case TT_COMPILER_CREATED:
            return "TT_COMPILER_CREATED";
        case TT_BIT_AND:
            return "TT_BIT_AND";
        case TT_BIT_OR:
            return "TT_BIT_OR";
        case TT_DECREMENT:
            return "TT_DECREMENT";
        case TT_INCREMENT:
            return "TT_INCREMENT";
        case TT_AND:
            return "TT_AND";
        case TT_OR:
            return "TT_OR";
        case TT_FIELD_ACCESSOR:
            return "TT_FIELD_ACCESSOR";
        case TT_POUND:
            return "TT_POUND";
        case TT_COLON:
            return "TT_COLON";
        case TT_VARARG:
            return "TT_VARARG";
        default:
            return "TT_VARARG";
    };
}

void Token::print() {
    auto *lex_value = token_type != TT_NEWLINE ? lexeme.c_str() : "\\n";
    printf("Token{%s, %s, %d}\n", this->to_string().c_str(), lex_value, linenum);
}

int Token::get_operator_precedence() {
    switch (token_type) {
        case TT_AND:
        case TT_OR:
            return -2;
        case TT_BIT_AND:
        case TT_BIT_OR:
            return -1;
        case TT_EQUALITY:
            return 0;
        case TT_LT:
        case TT_GT:
            return 1;
        case TT_GTE:
        case TT_LTE:
            return 2;
        case TT_ADD:
        case TT_SUB:
            return 3;
        case TT_STAR:
        case TT_DIV:
        case TT_MODULO: 
            return 4;
        case TT_EXPONENT:
            return 5;
        default:
            return -1;
    }
}
