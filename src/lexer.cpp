#include <string>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include "../include/error_logger.h"
#include "../include/lexer.h"
#include "../include/token.h"

using namespace std;

SageLexer::SageLexer(istream *stream, string sourcename): char_buffer(stream), sourcename(sourcename) {
    linenum = 1;
    linedepth = 0;
    current_token = new Token();
    last_token = Token();
    peeked_tokens = stack<Token>();
}

SageLexer::SageLexer() : char_buffer(nullptr) {
    sourcename = "";
    linenum = 0;
    linedepth = 0;
    current_token = new Token();
    last_token = Token();
    peeked_tokens = stack<Token>();
}

SageLexer::~SageLexer() {
    if (current_token != nullptr) {
        delete current_token;
        current_token = nullptr;
    }
}

Token *SageLexer::check_for_string() {
    string lexeme;
    if (current_char == '"') {
        lexeme += '"';
        char_buffer->get(current_char);
        linedepth++;

        while (current_char != '"' && current_char != '\n' && !char_buffer->eof()) {
            lexeme += current_char;
            char_buffer->get(current_char);
            linedepth++;
            if (char_buffer->eof() || char_buffer->fail()) break;
        }

        if (current_char != '"') {
            Token tok = Token(TT_STRING, lexeme, linenum);
            ErrorLogger::get().log_error_unsafe(tok, "Unterminated string literal.", SYNTAX);
        }

        lexeme += '"';
        return lexer_make_token(TT_STRING, lexeme);
    }

    return nullptr;
}

Token *SageLexer::check_for_character_literal() {
    string lexeme;
    if (current_char == '\'') {
        lexeme += '\'';
        char_buffer->get(current_char);
        linedepth++;
        char_buffer->get(current_char);
        linedepth++;

        if (current_char != '\'') {
            Token tok = Token(TT_CHARACTER_LITERAL, "\'", linenum);
            ErrorLogger::get().log_error_unsafe(tok, "Character literals cannot be more than one character long.", SYNTAX);
        }

        char_buffer->get(current_char);
        linedepth++;

        lexeme += '\'';
        return lexer_make_token(TT_CHARACTER_LITERAL, lexeme);
    }

    return nullptr;
}

Token *SageLexer::handle_symbol_case(
    char default_char, TokenType default_type,
    TokenType target_type, string target_symbol
) {
    Token *ret = followed_by(default_char, target_type, target_symbol);
    if (ret != nullptr) {
        return ret;
    }
    return lexer_make_token(default_type, string(1, default_char));
}

Token *SageLexer::lex_for_symbols() {
    Token *return_val = check_for_string();
    if (return_val != nullptr) {
        return return_val;
    }

    return_val = check_for_character_literal();
    if (return_val != nullptr) {
        return return_val;
    }

    unordered_map<char, TokenType> SYMBOLS = {
        {'(', TT_LPAREN},
        {')', TT_RPAREN},
        {'*', TT_MUL},
        {'/', TT_DIV},
        {'%', TT_MODULO},
        {',', TT_COMMA},
        {'[', TT_LBRACKET},
        {']', TT_RBRACKET},
        {'{', TT_LBRACE},
        {'}', TT_RBRACE},
        {'#', TT_POUND},
        {'@', TT_POINTER_DEREFERENCE}
    };

    char peekahead;
    char first_peek;
    switch (current_char) {
        case ':': {
            auto retval = followed_by(':', TT_BINDING, "::");
            if (retval != nullptr) {
                return retval;
            }

            return lexer_make_token(TT_COLON, ":");
        }
        case '^': {
            if (last_token.token_type == TT_NUM || last_token.token_type == TT_FLOAT) {
                return lexer_make_token(TT_EXPONENT, "^");
            }
            return lexer_make_token(TT_POINTER_REFERENCE, "^");
        }

        case '-':
            char_buffer->get(peekahead);
            if (peekahead == '>') {
                linedepth++;
                return lexer_make_token(TT_FUNC_RETURN_TYPE, "->");
            }
            if (peekahead == '-') {
                linedepth++;
                return lexer_make_token(TT_DECREMENT, "--");
            }

            char_buffer->putback(peekahead);
            return lexer_make_token(TT_SUB, "-");

        case '.':
            char_buffer->get(first_peek);
            current_char = first_peek;

            // if the '.' is in between an identifier token and unicode chars then its a field accessor
            // NOTE: isalpha(current_char) returns non zero value if the character is a letter
            if (last_token.token_type == TT_IDENT && (isalpha(current_char) != 0 || current_char == '_')) {
                char_buffer->putback(first_peek);
                return lexer_make_token(TT_FIELD_ACCESSOR, ".");
            }

            if (first_peek == '.') {
                char second_peek;
                char_buffer->get(second_peek);
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

Token *SageLexer::followed_by(char expected_char, TokenType expected_type, string expected_lexeme) {
    char_buffer->get(current_char);
    linedepth++;
    if (current_char == expected_char) {
        return lexer_make_token(expected_type, expected_lexeme);
    }

    char_buffer->putback(current_char);
    linedepth--;
    return nullptr;
}

Token *SageLexer::lexer_make_token(TokenType type, string lexeme, int depth) {
    current_token->lexeme = lexeme;
    current_token->token_type = type;
    current_token->filename = this->sourcename;
    current_token->linenum = this->linenum;
    current_token->linedepth = depth == -1 ? this->linedepth : depth;
    return current_token;
}

Token *SageLexer::lex_for_numbers() {
    if (!isdigit(current_char)) {
        return nullptr;
    }

    int dot_count = 0;
    string lexeme = "";
    TokenType tok_type = TT_NUM;
    while ((isdigit(current_char) || (current_char == '.' && dot_count == 0)) && !char_buffer->eof()) {
        if (current_char == '.') {
            tok_type = TT_FLOAT;
            dot_count++;
        }

        lexeme += string(1, current_char);
        char_buffer->get(current_char);
        linedepth++;

        if (char_buffer->eof() || char_buffer->fail()) {
            break;
        }
    }

    // if the number ends in a '.' then this is a number before the range operator and not actually a float number
    // we know this because the loop above handles floating point notation.
    if (lexeme.length() != 0 && lexeme[lexeme.length() - 1] == '.') {
        tok_type = TT_NUM;
        lexeme.pop_back();
        char_buffer->putback('.');
    }

    char_buffer->putback(current_char);
    return lexer_make_token(tok_type, lexeme);
}

Token *SageLexer::lex_for_identifiers() {
    if (!isalpha(current_char) && current_char != '_') {
        return nullptr;
    }

    string lexeme = "";

    set<string> KEYWORDS = {
        "int",
        "char",
        "void",
        "i32",
        "i64",
        "f32",
        "f64",
        "bool",
        "for",
        "while",
        "if",
        "else",
        "break",
        "continue",
        "fallthrough",
        "ret",
        "struct",
        "with", // previously: 'using'
        "run",
        "true",
        "false",
        "compact",
        "defer"
    };

    int starting_linedepth = linedepth;
    while ((isalpha(current_char) || isdigit(current_char) || current_char == '_') && !char_buffer->eof()) {
        lexeme += string(1, current_char);
        char_buffer->get(current_char);
        linedepth++;

        if (char_buffer->eof() || char_buffer->fail()) {
            break;
        }
    }

    char_buffer->putback(current_char);

    lexer_make_token(TT_IDENT, lexeme, starting_linedepth);
    if (KEYWORDS.find(lexeme) != KEYWORDS.end()) {
        current_token->token_type = TT_KEYWORD;
    }

    return current_token;
}

Token *SageLexer::get_token() {
    char_buffer->clear();
    Token *tok;

    // we check that the size is greater than one because the last_token will be held inside the buffer
    if (!peeked_tokens.empty()) {
        Token t = peeked_tokens.top();
        peeked_tokens.pop();

        current_token->fill_with(t);
        return current_token;
    }

    // note: CRITICAL!! We have to call get() before we check eof() becaus eof only updates to eof after a *failed* get() call.
    char_buffer->get(current_char);
    linedepth++;
    while ((current_char == ' ' || current_char == '\t') && !char_buffer->eof()) {
        char_buffer->get(current_char);
        linedepth++;
        if (char_buffer->eof() || char_buffer->fail()) break;
    }

    if (char_buffer->eof()) {
        tok = lexer_make_token(TT_EOF, "eof");
        last_token = *tok;
        return tok;
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

    string undefined_lexeme = str(current_char);
    tok = lexer_make_token(TT_ERROR, "unrecognized symbol");
    ErrorLogger::get().log_error_unsafe(*tok, sen("unrecognized symbol:", undefined_lexeme), SYNTAX);
    last_token = *tok;
    return tok;
}

void SageLexer::unget_token() {
    peeked_tokens.push(last_token);
}
