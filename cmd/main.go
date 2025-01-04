package main

import (
	"os"
	compiler "sage/internal/codegen"
)

func main() {
	sourcefile_name := string(os.Args[1:][0])
	compiler.BeginCodeCompilation(sourcefile_name)
}
