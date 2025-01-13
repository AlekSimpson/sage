#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "include/khash.h"
#include "include/lexer.h"
#include "include/token.h"
#include "include/stack.h"

KHASH_MAP_INIT_STR(symbol_map, TokenType);

Token* lexer_make_token(Lexer* lexer, TokenType type, const char* lexeme) {
    Token* tok = (Token*)malloc(sizeof(Token));
    tok->lexeme = lexeme;
    tok->type = type;
    tok->filename = lexer->filename;
    tok->linenum = lexer->linenum;
    tok->linedepth = lexer->linedepth;
    return tok;
}

Token* check_for_strings(Lexer* lexer) {
    char lexeme_buf[50];
    int lex_buf_len = 0;
    if (lexer->current_char == '"') {
        lexeme_buf[lex_buf_len] = '"';
        lex_buf_len++;

        lexer->current_char = pop(lexer->buffer).element.character;
        lexer->linedepth++;

        while (lexer->current_char != '"') {
            lexeme_buf[lex_buf_len] = lexer->current_char;
            lex_buf_len++;

            lexer->current_char = pop(lexer->buffer).element.character;
            lexer->linedepth++;
        }

        lexeme_buf[lex_buf_len] = '"';
        lex_buf_len++;

        // add this so that strcpy knows when to stop copying
        lexeme_buf[lex_buf_len] = '\0';
        lex_buf_len++;

        const char final_lexeme[lex_buf_len];
        strcpy(lexeme_buf, final_lexeme);

        // WARNING: MALLOC USED HERE, REQUIRES FREE()
        return lexer_make_token(lexer, TT_STRING, final_lexeme);
    }

    return NULL;
}

// Inserts items into hash map
int map_insert(khash_t(symbol_map)* map, const char* key, TokenType value) {
    int ret;
    khiter_t map_index;

    // Put key-value pair
    map_index = kh_put(symbol_map, map, key, &ret);  // ret indicates if new key
    if (ret > 0) {  // Key didn't exist
        kh_value(map, map_index) = value;
    }
    return ret;
}

Token* followed_by(Lexer* lexer, char expected_char, TokenType expected_type, char* expected_lexeme) {
    lexer->current_char = pop(lexer->buffer).element.character;
    lexer->linedepth++;
    if (lexer->current_char == expected_char) {
        return lexer_make_token(lexer, expected_type, expected_lexeme);
    }

    return NULL;
}

Token* handle_symbol_case(Lexer* lexer, char default_symbol, TokenType default_type, TokenType target_type, char* target_symbol) {
    Token* ret = followed_by(lexer, default_symbol, target_type, target_symbol);
    if (ret != NULL) {
        return ret;
    }

    const char default_sym[1] = {default_symbol}; // doing this to convert default_symbol into the right type for lexer_make_token

    return lexer_make_token(lexer, TT_ASSIGN, default_sym);
}

Token* check_for_symbols(Lexer* lexer) {
    Token* retval = check_for_strings(lexer);
    if (retval != NULL) {
        // if the return isn't NULL that means it did find a string
        return retval;
    }

    khash_t(symbol_map)* symbols = kh_init(symbol_map);

    const char keys[10] = {'(', ')', '*', '/', ',', '[', ']', '{', '}', '#'};
    TokenType values[10] = {TT_LPAREN, TT_RPAREN, TT_MUL, TT_DIV, TT_COMMA, TT_LBRACKET, TT_RBRACKET, TT_LBRACE, TT_RBRACE, TT_POUND};
    for (int i = 0; i < 10; ++i) {
        map_insert(symbols, &keys[i], values[i]);
    }

    char peekahead;
    char first_peek;
    Token* ret;
    khiter_t map_index;
    const char char_conv[1] = {lexer->current_char};
    switch (lexer->current_char) {
        case ':':
            return followed_by(lexer, ':', TT_BINDING, "::");
        case '-':
            peekahead = pop(lexer->buffer).element.character;
            if (peekahead == '>') {
                return lexer_make_token(lexer, TT_FUNC_RETURN_TYPE, "->");
            } else if (peekahead == '-') {
                return lexer_make_token(lexer, TT_DECREMENT, "--");
            }
            return lexer_make_token(lexer, TT_SUB, "-");
        case '.':
            first_peek = pop(lexer->buffer).element.character;
            lexer->current_char = first_peek;

            if (lexer->last_token.type == TT_IDENT && (isdigit(first_peek) || first_peek == '_')) {
                StackElement element;
                element.character = first_peek;
                push(lexer->buffer, element);

                return lexer_make_token(lexer, TT_FIELD_ACCESSOR, ".");
            }

            if (first_peek == '.') {
                char second_peek = pop(lexer->buffer).element.character;
                if (second_peek == '.') {
                    TokenType tok_type = TT_RANGE;
                    if (lexer->last_token.type == TT_IDENT) {
                        tok_type = TT_VARARG;
                    }

                    return lexer_make_token(lexer, tok_type, "...");
                }

                return NULL;
            }

            return NULL;

        case '=':
            return handle_symbol_case(lexer, '=', TT_ASSIGN, TT_EQUALITY, "==");
        case '+':
            return handle_symbol_case(lexer, '+', TT_ADD, TT_INCREMENT, "++");
        case '>':
            return handle_symbol_case(lexer, '>', TT_GT, TT_GTE, ">=");
        case '<':
            return handle_symbol_case(lexer, '<', TT_LT, TT_LTE, "<=");
        case '&':
            return handle_symbol_case(lexer, '&', TT_BIT_AND, TT_AND, "&&");
        case '|':
            return handle_symbol_case(lexer, '|', TT_BIT_OR, TT_OR, "||");
        default:
            map_index = kh_get(symbol_map, symbols, char_conv);

            if (map_index == kh_end(symbols)) {
                // if the char doesn't exist in the map then return NULL
                return NULL;
            }

            TokenType tok_type = kh_value(symbols, map_index);
            return lexer_make_token(lexer, tok_type, char_conv);
    }
}

Token* lexer_for_numbers(Lexer* lexer) {
    if (!isdigit(lexer->current_char)) {
        return NULL;
    }

    int dot_count = 0;
    char lexeme_buf[50];
    int lexeme_buf_len = 0;
    TokenType tok_type = TT_NUM;

    while (isdigit(lexer->current_char) || (lexer->current_char == '.' && dot_count == 0)) {
        if (lexer->current_char == '.') {
            tok_type = TT_FLOAT;
            dot_count++;
        }

        lexeme_buf[lexeme_buf_len] = lexer->current_char;
        lexeme_buf_len++;

        lexer->current_char = pop(lexer->buffer).element.character;
        lexer->linedepth++;
    }

	// if the number ends in a '.' then this is a number before the range operator and not actually a float number
	// we know this because the loop above handles floating point notation.
    if (lexeme_buf[lexeme_buf_len-1] == '.') {
		// in which case we want to set it back to a regular number, remove the last period character and put it back in the char buffer.
        tok_type = TT_NUM;
        lexeme_buf_len--;
        // put the '.' back in the buffer because we want to properly part the symbol next
        StackElement element;
        element.character = '.';
        push(lexer->buffer, element);
    }

    // add the last read char back onto to the top of the buffer because it was not matched to the current Token.
    StackElement element;
    element.character = lexer->current_char;
    push(lexer->buffer, element);

    // add null terminator so that strcpy knows when to stop copying
    lexeme_buf[lexeme_buf_len] = '\0';
    const char* final;
    strcpy(lexeme_buf, final);
    return lexer_make_token(lexer, tok_type, final);
}

Token* lex_for_identifiers(Lexer* lexer) {
    if (!isalpha(lexer->current_char) || lexer->current_char != '_') {
        return NULL;
    }

    char lexeme_buf[50];
    int lexeme_buf_len = 0;

    while (isalpha(lexer->current_char) || isdigit(lexer->current_char) || lexer->current_char == '_') {
        lexeme_buf[lexeme_buf_len] = lexer->current_char;
        lexeme_buf_len++;
        lexer->current_char = pop(lexer->buffer).element.character;
        lexer->linedepth++;
    }

    // Add the last read char back onto to the top of the buffer because it was not matched to the current Token
    StackElement element;
    element.character = lexer->current_char;
    push(lexer->buffer, element);

    lexeme_buf[lexeme_buf_len] = '\0';

    const char* final_lexeme;
    strcpy(lexeme_buf, final_lexeme);

    // check if lexeme_buf is a keyword or just a regular identifier
    switch (final_lexeme) {
    }
}











