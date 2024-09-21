package main


type Parser struct {
    tokens   *Queue[Token]
    curr_tok Token
}

func (p *Parser) NewParser(tokens []Token) *Parser {
    token_buffer := NewQueue(tokens)

    return &Parser{
        tokens: token_buffer,
        curr_tok: token_buffer.pop(),
    }
}

func (p *Parser) generateParseTree() {
}

// EXPRESSION
// COMPARISON
// UNARY
// MULT/DIV
// ADD/SUB
// NUMBER/ID/EXPRESSION
