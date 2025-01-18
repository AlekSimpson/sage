#ifndef TOKEN
#define TOKEN

#include <string>

typedef enum {
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
    TT_ASSIGN ,
    TT_LPAREN ,
    TT_RPAREN ,
    TT_LBRACE ,
    TT_RBRACE ,
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
} TokenType;

class Token {
public:
    string lexeme;
    string filename;
    TokenType type;
    int linenum;
    int linedepth;

    Token(TokenType type, string lexeme, int linenum);
    Token(string err_message, int linenum);
    ~Token();
    operator string();
    void print();
    int get_operator_precedence();
}

#endif
