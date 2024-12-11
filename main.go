package main

import (
	"os"
	"strings"
)

func main() {
	sourcefile_name := string(os.Args[1:][0])
	del := strings.Split(sourcefile_name, ".")
	if len(del) != 2 || del[1] != "g" {
		panic("Cannot parse inputted file type")
	}

	contents, err := os.ReadFile(sourcefile_name)
	if err != nil {
		panic(err)
	}

	queue := NewQueue(contents, 0)
	lexer := Lexer{queue, []Token{}, 0, ' '}
	tokens := lexer.lex()

	parser := NewParser(tokens)
	tree := parser.parse_program()
	tree.showtree("")
}
