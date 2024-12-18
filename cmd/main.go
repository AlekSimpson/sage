package main

import (
	"os"
	"sage/internal/parser"
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

	parser := sage.NewParser(contents)
	tree := parser.Parse_program()
	if tree != nil {
		tree.Showtree("")
	}
}
