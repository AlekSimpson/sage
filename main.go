package main

import (
    "strings"
    "os"
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

    queue := NewQueue(contents)
    lexer := Lexer{queue, []Token{}, 0, ' '}
    tokens := lexer.lex()

    parser := NewParser(tokens)
   
}
