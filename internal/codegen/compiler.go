package sage

import (
	"sage/internal/parser"
)

// TODO: Compile time run statements REQUIRE the interpreter to be working first
func Compile_code(code_content []byte, filename string) {
	parser := sage.NewParser(code_content)
	parsetree := parser.Parse_program()

	// Create IR module from parse tree
	code_module := NewIRModule(filename)

	// loop through root node chilren
	// generate IR nodes for each child node
}
