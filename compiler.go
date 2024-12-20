package sage

import (
	"fmt"
	"sage/internal/parser"
)

func CompileCode(code_contents []byte) {
	parser := sage.NewParser(code_contents)
	parsetree := parser.Parse_program()
	fmt.Println(parsetree.String())

}
