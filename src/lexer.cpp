#include <string>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include "../include/lexer.h"
#include "../include/token.h"

using namespace std;

Lexer::Lexer(string fname) {
    filename = fname;
    linenum = 0;
    linedepth = 0;
    current_token = new Token();
    char_buffer.open(fname);
    last_token = Token();
    peeked_tokens = stack<Token>();
}

Lexer::~Lexer() {
    delete current_token;
    if (char_buffer.is_open()) {
        char_buffer.close();
    }
}

Token* Lexer::check_for_string() {
    string lexeme;
    if (current_char == '"') {
        lexeme += '"';
        char_buffer.get(current_char);
        linedepth++;

        while (current_char != '"') {
            lexeme += current_char;
            char_buffer.get(current_char);
            linedepth++;
        }

        lexeme += '"';
        return lexer_make_token(TT_STRING, lexeme);
    }

    return nullptr;
}

Token* Lexer::handle_symbol_case(
    char default_char, TokenType default_type, 
    TokenType target_type, string target_symbol
) {
	Token* ret = followed_by(default_char, target_type, target_symbol);
	if (ret != nullptr) {
		return ret;
	}
	return lexer_make_token(default_type, string(1, default_char));
}

Token* Lexer::lex_for_symbols() {
    Token* return_val = check_for_string();
    if (return_val != nullptr) {
        return return_val;
    }

    unordered_map<char, TokenType> SYMBOLS = {
        {'(', TT_LPAREN},
        {')', TT_RPAREN},
        {'*', TT_MUL},
        {'/', TT_DIV},
        {',', TT_COMMA},
        {'[', TT_LBRACKET},
        {']', TT_RBRACKET},
        {'{', TT_LBRACE},
        {'}', TT_RBRACE},
        {'#', TT_POUND},
    }; 

    char peekahead;
    char first_peek;
    Token last_token;
    switch (current_char) {
        case ':':
            return followed_by(':', TT_BINDING, "::");

        case '-':
            char_buffer.get(peekahead);
            if (peekahead == '>') {
                return lexer_make_token(TT_FUNC_RETURN_TYPE, "->");
            }else if (peekahead == '-') {
                return lexer_make_token(TT_DECREMENT, "--");
            }

            return lexer_make_token(TT_SUB, "-");

        case '.':
            char_buffer.get(first_peek);
            current_char = first_peek;

            // if the '.' is in between an identifier token and unicode chars then its a field accessor
            if (last_token.token_type == TT_IDENT && (isalpha(current_char) || current_char == '_')) {
                char_buffer.putback(first_peek);
                return lexer_make_token(TT_FIELD_ACCESSOR, ".");
            }

            if (first_peek == '.') {
                char second_peek;
                char_buffer.get(second_peek);
                if (second_peek == '.') {
                    TokenType tok_type = TT_RANGE;
                    if (last_token.token_type == TT_IDENT) {
                        tok_type = TT_VARARG;
                    }

                    return lexer_make_token(tok_type, "...");
                }
            }

            return nullptr;

        case '=':
            return handle_symbol_case('=', TT_ASSIGN, TT_EQUALITY, "==");

        case '+':
            return handle_symbol_case('+', TT_ADD, TT_INCREMENT, "++");

        case '>':
            return handle_symbol_case('>', TT_GT, TT_GTE, ">=");

        case '<':
            return handle_symbol_case('<', TT_LT, TT_LTE, "<=");

        case '&':
            return handle_symbol_case('&', TT_AND, TT_BIT_AND, "&&");

        case '|':
            return handle_symbol_case('|', TT_OR, TT_BIT_OR, "||");

        default:
            if (SYMBOLS.find(current_char) == SYMBOLS.end()) {
                return nullptr;
            }

            return lexer_make_token(SYMBOLS[current_char], string(1, current_char));
    }
}

Token* Lexer::followed_by(char expected_char, TokenType expected_type, string expected_lexeme) {
    char_buffer.get(current_char);
    linedepth++;
    if (current_char == expected_char) {
        return lexer_make_token(expected_type, expected_lexeme);
    }

    return nullptr;
}

Token* Lexer::lexer_make_token(TokenType type, string lexeme) {
    current_token->lexeme = lexeme;
    current_token->token_type = type;
    current_token->filename = this->filename;
    current_token->linenum = this->linenum;
    current_token->linedepth = this->linedepth;
    return current_token;
}

Token* Lexer::lex_for_numbers() {
    if (!isdigit(current_char)) {
        return nullptr;
    }

    int dot_count = 0;
    string lexeme = "";
    TokenType tok_type;
    while (isdigit(current_char) || (current_char == '.' && dot_count == 0)) {
        if (current_char == '.') {
            tok_type = TT_FLOAT;
            dot_count++;
        }

        lexeme += string(1, current_char);
        char_buffer.get(current_char);
        linedepth++;
    }

    // if the number ends in a '.' then this is a number before the range operator and not actually a float number
    // we know this because the loop above handles floating point notation.
    if (lexeme.length() != 0 && lexeme[lexeme.length()-1] == '.') {
        tok_type = TT_NUM;
        lexeme.pop_back();
        char_buffer.putback('.');
    }

    char_buffer.putback(current_char);
    return lexer_make_token(tok_type, lexeme);
}

Token* Lexer::lex_for_identifiers() {
    if (!isalpha(current_char) && current_char != '_') {
        return nullptr;
    }

    string lexeme = "";

    unordered_map<string, int> KEYWORDS = {
        {"int", 0},
        {"char", 1},
        {"void", 2},
        {"i16", 3},
        {"i32", 4},
        {"i64", 5},
        {"f32", 6},
        {"f64", 7},
        {"bool", 8},
        {"include", 9},
        {"for", 10},
        {"while", 11},
        {"in", 12},
        {"if", 13},
        {"else", 14},
        {"break", 15},
        {"continue", 16},
        {"fallthrough", 17}, // like the break keyword but for nested loops, if used inside a nested loop it will break out of all loops
        {"ret", 18},
        {"struct", 19},
        {"using", 20},
        {"execute", 21},
    };

    while (isalpha(current_char) || isdigit(current_char) || current_char == '_') {
        lexeme += string(1, current_char);
        char_buffer.get(current_char);
        linedepth++;
    }

    char_buffer.putback(current_char);

    lexer_make_token(TT_IDENT, lexeme);
    if (KEYWORDS.find(lexeme) == KEYWORDS.end()) {
        current_token->token_type = TT_KEYWORD;
    }

    return current_token;
}

Token* Lexer::get_token() {
    Token* tok;

    // we check that the size is greater than one because the last_token will be held inside the buffer
    if (!peeked_tokens.empty()) {
        Token tok = peeked_tokens.top();
        peeked_tokens.pop();

        current_token->fill_with(tok);
        return current_token;
    }

    if (char_buffer.eof()) {
        tok = lexer_make_token(TT_EOF, "eof");
        last_token = *tok;
        return tok;
    }

    char_buffer.get(current_char);
    linedepth++;

    if (current_char == ' ' || current_char == '\t') {
        char_buffer.get(current_char);
        linedepth++;
    }

    if (current_char == '\n') {
        tok = lexer_make_token(TT_NEWLINE, "\n");
        linenum++;
        linedepth = 0;
        last_token = *tok;
        return tok;
    }

    tok = lex_for_symbols();
    if (tok != nullptr) {
        last_token = *tok;
        return tok;
    }

    tok = lex_for_identifiers();
    if (tok != nullptr) {
        last_token = *tok;
        return tok;
    }

    tok = lex_for_numbers();
    if (tok != nullptr) {
        last_token = *tok;
        return tok;
    }

    tok = lexer_make_token(TT_ERROR, "unrecognized symbol");
    last_token = *tok;
    return tok;
}

void Lexer::unget_token() {
    peeked_tokens.push(last_token);
}
