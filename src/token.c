#include <stdio.h>
#include <string.h>
#include "include/token.h"

int token_precedence(Token* token) {
    switch (token->type) {
        case TT_EQUALITY: return 0;
        case TT_LT: return 1;
        case TT_GT: return 1;
        case TT_GTE: return 2;
        case TT_LTE: return 2;
        case TT_ADD: return 3;
        case TT_SUB: return 3;
        case TT_MUL: return 4;
        case TT_DIV: return 4;
        case TT_EXP: return 5;
        default: return -1;
    }
}

const char* token_name(Token* token) {
    string str;
    switch (token->type) {
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
        case TT_MUL:
            return "TT_MUL";
        case TT_DIV:
            return "TT_DIV";
        case TT_EXP:
            return "TT_EXP";
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
        case TT_STAR:
            return "TT_STAR";
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
            return "TT_UNRECOGNIZED";
    }
} 

// buffer needs to have its size calculcated ahead of time
void token_show(char* buffer, Token* token) {
    sprintf(buffer, "Token{%s, %s, %d}", token_name(token), token->filename, token->linenum);
}

void token_print(Token* token) {
    int tok_name_len = strlen(token_name(token));
    int filename_len = strlen(token->filename);
    int buf_len = 11 + tok_name_len + filename_len;
    char buffer[buf_len];

    token_show(buffer, token);

    printf("%s\n", buffer);
}
