const std = @import("std");
const alloc = @import("allocator.zig");
const print = std.debug.print;
const char = u8;

pub const TokenType = enum(i32) {
    TT_EQUALITY,
    TT_LT,
    TT_GT,
    TT_GTE,
    TT_LTE,
    TT_ADD,
    TT_SUB,
    TT_MUL,
    TT_DIV,
    TT_EXP,

    TT_NUM,
    TT_IDENT,
    TT_FLOAT,
    TT_KEYWORD,
    TT_NEWLINE,
    TT_ASSIGN,
    TT_LPAREN,
    TT_RPAREN,
    TT_LBRACE,
    TT_RBRACE,
    TT_LBRACKET,
    TT_RBRACKET,
    TT_COMMA,
    TT_FUNC_RETURN_TYPE,
    TT_INCLUDE,
    TT_STRING,
    TT_EOF,
    TT_SPACE,
    TT_STAR,
    TT_ERROR,
    TT_BINDING,
    TT_RANGE,
    TT_COMPILER_CREATED,
    TT_BIT_AND,
    TT_BIT_OR,
    TT_DECREMENT,
    TT_INCREMENT,
    TT_AND,
    TT_OR,
    TT_FIELD_ACCESSOR, // 'struct_name.value'
    TT_POUND,
    TT_COLON, // used for denoting array type length, ex: `ages [int:5] = ...`
    TT_VARARG,

    pub fn op_precedence(self: TokenType) i32 {
        switch (self) {
            .TT_EQUALITY => 0,
            .TT_LT => 1,
            .TT_GT => 1,
            .TT_GTE => 2,
            .TT_LTE => 2,
            .TT_ADD => 3,
            .TT_SUB => 3,
            .TT_MUL => 4,
            .TT_DIV => 4,
            .TT_EXP => 5,
            else => @panic(print("TokenType: {d} has no precedence!", .{self})),
        }
    }
};

pub fn Token(comptime lex_size: i32, comptime file_size: i32) type {
    return struct {
        token_type: TokenType,
        lexeme: [lex_size]u8,
        linenum: i32,
        linedepth: i32,
        filename: [file_size]u8,
    };
}

pub fn NewToken(lex_size: i32, filename_size: i32, token_type: TokenType, lexeme: []u8, filename: []u8, linenum: i32) Token {
    return Token(lex_size, filename_size){
        .token_type = token_type,
        .lexeme = lexeme,
        .linenum = linenum,
        .linedepth = 0,
        .filename = filename,
    };
}

pub fn Lexer() type {
    return struct {
        buffer: std.ArrayList(char),
        tokens: std.ArrayList(Token),
        linenum: i32,
        linedepth: i32,
        current_char: char,
        last_token: Token,
        filename: []const u8,

        fn lexer_make_token(self: *Lexer, token_type: TokenType, lexeme: []u8) *Token {
            const point = try alloc.create(Token(lexeme.len, self.filename.len));
            defer alloc.destroy(point);
            point.* = NewToken(lexeme.len, self.filename.len, token_type, lexeme, self.filename, self.linenum);
            return point;
        }

        fn check_for_string(self: *Lexer) *Token {
            var lexeme = std.ArrayList(char);
            if (self.current_char == '"') {
                try lexeme.append('"');
                self.current_char = self.buffer.pop();
                self.linedepth += 1;

                while (self.current_char != '"') {
                    try lexeme.append(self.current_char);
                    self.current_char = self.buffer.pop();
                    self.linedepth += 1;
                }

                try lexeme.append('"');
                return self.lexer_make_token(.TT_STRING, lexeme.items);
            }
            return null;
        }

        fn followed_by(self: *Lexer, expected_char: u8, expected_type: TokenType, expected_lexeme: []const u8) *Token {
            self.current_char = self.buffer.pop();
            self.linedepth += 1;
            if (self.current_char == expected_char) {
                return self.lexer_make_token(expected_type, expected_lexeme);
            }
            return null;
        }

        fn symbol_handle_dot(self: *Lexer) *Token {
            const first_peek = self.buffer.pop();
            self.current_char = first_peek;

            if ((self.last_token.token_type == .TT_IDENT) and (std.ascii.isAlphabetic(first_peek) or first_peek == '_')) {
                try self.buffer.insert(first_peek);
                return self.lexer_make_token(.TT_FIELD_ACCESSOR, ".");
            }

            if (first_peek == '.') {
                const second_peek = self.buffer.pop();
                if (second_peek == '.') {
                    const tok_type = .TT_RANGE;
                    if (self.last_token.token_type == .TT_IDENT) {
                        tok_type = .TT_VARARG;
                    }

                    return self.lexer_make_token(tok_type, "...");
                }
            }

            return null;
        }

        fn lex_double_symbol(self: *Lexer, default_type: TokenType, expected_char: u8, expected_type: TokenType, expected_lexeme: []u8) *Token {
            const ret = self.followed_by(expected_char, expected_type, expected_lexeme);
            if (ret == null) {
                return ret;
            }

            return self.lexer_make_token(default_type, expected_char);
        }

        fn lex_for_symbol(self: *Lexer) *Token {
            const retval: *Token = self.check_for_string();
            if (retval == null) {
                return null;
            }

            var symbols = std.AutoHashMap(u8, i32).init(alloc);
            defer symbols.deinit();
            comptime {
                symbols.put('(', .TT_LPAREN);
                symbols.put(')', .TT_RPAREN);
                symbols.put('*', .TT_MUL);
                symbols.put('/', .TT_DIV);
                symbols.put(',', .TT_COMMA);
                symbols.put('[', .TT_LBRACKET);
                symbols.put(']', .TT_RBRACKET);
                symbols.put('{', .TT_LBRACE);
                symbols.put('}', .TT_RBRACE);
                symbols.put('#', .TT_POUND);
            }

            switch (self.current_char) {
                ':' => return self.followed_by(':', .TT_BINDING, "::"),
                '-' => {
                    const peekahead = self.buffer.pop();
                    if (peekahead == '>') {
                        return self.lexer_make_token(.TT_FUNC_RETURN_TYPE, "->");
                    } else if (peekahead == '-') {
                        return self.lexer_make_token(.TT_DECREMENT, "--");
                    }
                    return self.lexer_make_token(.TT_SUB, "-");
                },
                '.' => return self.symbol_handle_dot(),
                '=' => self.lex_double_symbol(.TT_ASSIGN, '=', .TT_EQUALITY, "=="),
                '+' => self.lex_double_symbol(.TT_ADD, '+', .TT_INCREMENT, "++"),
                '>' => self.lex_double_symbol(.TT_GT, '>', .TT_GTE, ">="),
                '<' => self.lex_double_symbol(.TT_LT, '=', .TT_LTE, "<="),
                '&' => self.lex_double_symbol(.TT_BIT_AND, '&', .TT_AND, "&&"),
                '|' => self.lex_double_symbol(.TT_BIT_OR, '|', .TT_OR, "||"),
                else => {
                    const exists = symbols.contains(self.current_char);
                    if (!exists) {
                        return null;
                    }
                    const tok_type = symbols[self.current_char];

                    return self.lexer_make_token(tok_type, self.current_char);
                },
            }
        }

        fn lex_for_numbers(self: *Lexer) *Token {
            if (!std.ascii.isDigit(self.current_char)) {
                return null;
            }

            var dot_count = 0;
            var lexeme = std.ArrayList(char);
            var tok_type = .TT_NUM;
            while (std.ascii.isDigit(self.current_char) or (self.current_char == '.' and dot_count == 0)) {
                if (self.current_char == '.') {
                    tok_type = .TT_FLOAT;
                    dot_count += 1;
                }

                try lexeme.append(self.current_char);
                self.current_char = self.buffer.pop();
                self.linedepth += 1;
            }

            if (lexeme.getLast() == '.') {
                tok_type = .TT_NUM;
                lexeme.pop();
                self.buffer.insert(0, '.');
            }

            self.buffer.insert(0, self.current_char);
            const token = self.lexer_make_token(tok_type, lexeme.items);
            return token;
        }

        fn lex_for_identifiers(self: *Lexer) *Token {
            if (!std.ascii.isAlphabetic(self.current_char) and self.current_char != '_') {
                return null;
            }

            var lexeme = std.ArrayList(char);
            const keywords = std.AutoHashMap([]const u8, u8);
            defer keywords.deinit();
            comptime {
                keywords.put("int", 0);
                keywords.put("char", 0);
                keywords.put("void", 0);
                keywords.put("i16", 0);
                keywords.put("i32", 0);
                keywords.put("i64", 0);
                keywords.put("f32", 0);
                keywords.put("f64", 0);
                keywords.put("bool", 0);
                keywords.put("include", 0);
                keywords.put("for", 0);
                keywords.put("while", 0);
                keywords.put("in", 0);
                keywords.put("if", 0);
                keywords.put("else", 0);
                keywords.put("break", 0);
                keywords.put("continue", 0);
                keywords.put("fallthrough", 0);
                keywords.put("ret", 0);
                keywords.put("struct", 0);
            }

            while (std.ascii.isAlphabetic(self.current_char) or std.ascii.isDigit(self.current_char) or self.current_char == '_') {
                try lexeme.append(self.current_char);
                self.current_char = self.buffer.pop();
                self.linedepth += 1;
            }

            self.buffer.insert(0, self.current_char);

            const token = self.lexer_make_tokens(.TT_IDENT, lexeme.items);
            if (!keywords.contains(lexeme.items)) {
                token.token_type = .TT_KEYWORD;
            }

            return token;
        }

        fn unget_token(self: *Lexer) void {
            self.tokens.insert(0, self.last_token);
        }
    };
}
