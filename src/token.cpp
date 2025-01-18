#include <string>
#include <cstdlib>
#include <cstdio>
#include "include/token.h"

Token::Token(TokenType type, string lexeme, int linenum) {
    this->lexeme = lexeme;
    this->type = type;
    this->linenum = linenum;
}

Token::Token(string err_message, int linenum) {
    this->token_type = TT_ERROR; 
    this->linenum = linenum;
}

~Token::Token() {
    delete this;
}

operator Token::string() {
    switch (this->token_type) {
        case TT_EQUALITY:
            return "TT_EQUALITY";
            break;
        case TT_LT:
            return "TT_LT";
            break;
        case TT_GT:
            return "TT_GT";
            break;
        case TT_GTE:
            return "TT_GTE";
            break;
        case TT_LTE:
            return "TT_LTE";
            break;
        case TT_ADD:
            return "TT_ADD";
            break;
        case TT_SUB:
            return "TT_SUB";
            break;
        case TT_MUL:
            return "TT_MUL";
            break;
        case TT_DIV:
            return "TT_DIV";
            break;
        case TT_EXP:
            return "TT_EXP";
            break;
        case TT_NUM:
            return "TT_NUM";
            break;
        case TT_IDENT:
            return "TT_IDENT";
            break;
        case TT_FLOAT:
            return "TT_FLOAT";
            break;
        case TT_KEYWORD:
            return "TT_KEYWORD";
            break;
        case TT_NEWLINE:
            return "TT_NEWLINE";
            break;
        case TT_ASSIGN:
            return "TT_ASSIGN";
            break;
        case TT_LPAREN:
            return "TT_LPAREN";
            break;
        case TT_RPAREN:
            return "TT_RPAREN";
            break;
        case TT_LBRACE:
            return "TT_LBRACE";
            break;
        case TT_RBRACE:
            return "TT_RBRACE";
            break;
        case TT_LBRACKET:
            return "TT_LBRACKET";
            break;
        case TT_RBRACKET:
            return "TT_RBRACKET";
            break;
        case TT_COMMA:
            return "TT_COMMA";
            break;
        case TT_FUNC_RETURN_TYPE:
            return "TT_FUNC_RETURN_TYPE";
            break;
        case TT_INCLUDE:
            return "TT_INCLUDE";
            break;
        case TT_STRING:
            return "TT_STRING";
            break;
        case TT_EOF:
            return "TT_EOF";
            break;
        case TT_SPACE:
            return "TT_SPACE";
            break;
        case TT_STAR:
            return "TT_STAR";
            break;
        case TT_ERROR:
            return "TT_ERROR";
            break;
        case TT_BINDING:
            return "TT_BINDING";
            break;
        case TT_RANGE:
            return "TT_RANGE";
            break;
        case TT_COMPILER_CREATED:
            return "TT_COMPILER_CREATED";
            break;
        case TT_BIT_AND:
            return "TT_BIT_AND";
            break;
        case TT_BIT_OR:
            return "TT_BIT_OR";
            break;
        case TT_DECREMENT:
            return "TT_DECREMENT";
            break;
        case TT_INCREMENT:
            return "TT_INCREMENT";
            break;
        case TT_AND:
            return "TT_AND";
            break;
        case TT_OR:
            return "TT_OR";
            break;
        case TT_FIELD_ACCESSOR:
            return "TT_FIELD_ACCESSOR";
            break;
        case TT_POUND:
            return "TT_POUND";
            break;
        case TT_COLON:
            return "TT_COLON";
            break;
        case TT_VARARG:
            return "TT_VARARG";
            break;
        default:
            return "TT_VARARG";
            break;
    };
}

void Token::print() {
    printf(this->string());
}

int Token::get_operator_precedence() {
    switch (this->token_type) {
        case TT_EQUALITY:
            return 0;
            break;
        case TT_LT:
            return 1;
            break;
        case TT_GT:
            return 1;
            break;
        case TT_GTE:
            return 2;
            break;
        case TT_LTE:
            return 2;
            break;
        case TT_ADD:
            return 3;
            break;
        case TT_SUB:
            return 3;
            break;
        case TT_MUL:
            return 4;
            break;
        case TT_DIV:
            return 4;
            break;
        case TT_EXP:
            return 5;
            break;
        default:
            return -1;
            break;
    }
}
