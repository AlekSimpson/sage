package main

import (
	"os"
	// parser "sage/internal/parser"
	compiler "sage/internal/codegen"
	"strings"
)

func main() {
	sourcefile_name := string(os.Args[1:][0])
	del := strings.Split(sourcefile_name, ".")
	if len(del) != 2 || (del[1] != "g" && del[1] != "sage") {
		panic("Cannot parse inputted file type")
	}

	contents, err := os.ReadFile(sourcefile_name)
	if err != nil {
		panic(err)
	}

	compiler.BeginCodeCompilation(contents, del[0])

	// parser := parser.NewParser(contents)
	// tree := parser.Parse_program()
	// if tree != nil {
	// 	tree.Showtree("")
	// }
}
