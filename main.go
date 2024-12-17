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
	lexer := NewLexer(queue)

	parser := NewParser(lexer)
	tree := parser.parse_program()
	if tree != nil {
		tree.showtree("")
	}
}
