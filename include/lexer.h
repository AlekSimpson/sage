#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stack>
#include "token.h"
using namespace std;

class SageLexer {
public:
    Token last_token;
    stack<Token> peeked_tokens;
    //ifstream char_buffer;
    istream &char_buffer;
    Token *current_token;
    string sourcename;
    int linenum;
    int linedepth;
    char current_char;

    SageLexer(istream &stream, string sourcename);
    //SageLexer();
    ~SageLexer();

    Token *get_token();
    void unget_token();

private:
    Token *lexer_make_token(TokenType type, string lexeme, int depth = -1);
    Token *check_for_string();
    Token *handle_symbol_case(
        char default_char, TokenType default_type,
        TokenType target_type, string target_symbol
    );
    Token *lex_for_symbols();
    Token *followed_by(char, TokenType, string);
    Token *lex_for_numbers();
    Token *lex_for_identifiers();
};
