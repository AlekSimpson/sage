#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <cctype>
#include <unordered_map>
#include "include/lexer.h"
#include "include/token.h"

Lexer::Lexer(string filename, stack<char> buffer) {
    filename = filename;
    linenum = 0;
    linedepth = 0;
    char_buffer = buffer;
    tokens = new stack<char>(); // WARNING: needs a delete in the destructor
}

void Lexer::update_last_token() {
    Token* copy = new Token;
    *copy = *current_token;

    // we are only keeping track of the last token
    // so we first pop the old last token
    tokens.pop();
    // then opdate it with the new one
    tokens.push(copy);
}

Token* Lexer::check_for_string() {
    string lexeme;
    if (current_char == '"') {
        lexeme += '"';
        current_char = char_buffer.pop();
        linedepth++;

        while (current_char != '"') {
            lexeme += current_char;
            current_char = char_buffer.pop();
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
	Token* ret = followed_by('+', t.TT_INCREMENT, "++");
	if (ret != nil) {
		return ret;
	}
	return lexer_make_token(t.TT_ADD, "+");
}

Token* Lexer::lex_for_symbols() {
    Token* return_val = check_for_string();
    if (return_val != nullptr) {
        return return_val;
    }

    unordered_map<char, TokenType> SYMBOLS = {
        {'(', t.TT_LPAREN},
        {')', t.TT_RPAREN},
        {'*', t.TT_MUL},
        {'/', t.TT_DIV},
        {',', t.TT_COMMA},
        {'[', t.TT_LBRACKET},
        {']', t.TT_RBRACKET},
        {'{', t.TT_LBRACE},
        {'}', t.TT_RBRACE},
        {'#', t.TT_POUND},
    }; 

    switch (current_char) {
        case ':':
            return followed_by(':', TT_BINDING, "::");
        case '-':
            char peekahead = char_buffer.pop();
            if (peekahead == '>') {
                return lexer_make_token(TT_FUNC_RETURN_TYPE, "->");
            }else if (peekahead == '-') {
                return lexer_make_token(TT_DECREMENT, "--");
            }

            return lexer_make_token(TT_SUB, "-");
        case '.':
            char first_peek = char_buffer.pop();
            current_char = first_peek;

            // if the '.' is in between an identifier token and unicode chars then its a field accessor
            Token& last_token = tokens.top();
            if (last_token.token_type == TT_IDENT && (isalpha(current_char) || current_char == '_')) {
                char_buffer.push(first_peek);
                return lexer_make_token(TT_FIELD_ACCESSOR, ".");
            }

            if (first_peek == '.') {
                char second_peek = char_buffer.pop();
                if (second_peek == '.') {
                    TokenType tok_type = TT_RANGE;
                    if (last_token.token_type == TT_IDENT) {
                        tok_type = TT_VARARG;
                    }

                    return lexer_make_token(tok_type, "...");
                }

                return nullptr;
            }

            return nullptr;

            j
    }
}

Token* Lexer::get_token() {
    Token* tok;

    // we check that the size is greater than one because the last_token will be held inside the buffer
    if (!(tokens.size() > 1)) {
        return tokens.pop();
    }

    if (char_buffer.empty()) {
        tok = lexer_make_token(TT_EOF, "eof");
        update_last_token();
        return tok;
    }

    current_char = char_buffer.pop();
    linedepth++;

    if (current_char == ' ' || current_char == '\t') {
        current_char = char_buffer.pop();
        linedepth++;
    }

    if (current_char == '\n') {
        tok = lexer_make_token(TT_NEWLINE, "\n");
        linenum++;
        linedepth = 0;
        update_last_token();
        return tok;
    }

    tok = lex_for_symbols();
    if (tok != nullptr) {
        update_last_token();
        return tok;
    }

    tok = lex_for_identifiers();
    if (tok != nullptr) {
        update_last_token();
        return tok;
    }

    tok = lex_for_numbers();
    if (tok != nullptr) {
        update_last_token();
        return tok;
    }

    tok = lexer_make_token(TT_ERROR, "unrecognized symbol");
    update_last_token();
    return tok;
}

void Lexer::unget_token(Token* tok) {
    tokens.push(tok)
}
