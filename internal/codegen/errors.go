package sage

import (
	"fmt"
	// "github.com/jedib0t/go-pretty/v6/text"
	"sage/internal/parser"
	"strings"
)

type SG_ERROR int

const (
	SYNTAX_ERR SG_ERROR = iota
	SEMANTIC_ERR
	LEXICAL_ERR
	NAME_RESOLUTION_ERR
	TYPE_CHECK_ERR
	MEMORY_ERR
	LINKER_ERR
)

func err_type_to_title(err SG_ERROR) string {
	errmap := map[SG_ERROR]string{
		SYNTAX_ERR:          "Syntax Error",
		SEMANTIC_ERR:        "Semantic Error",
		LEXICAL_ERR:         "Lexical Error",
		NAME_RESOLUTION_ERR: "Name Resolution Error",
		TYPE_CHECK_ERR:      "Type Check Error",
		MEMORY_ERR:          "Memory Error",
		LINKER_ERR:          "Linker Error",
	}

	return errmap[err]
}

type SageError struct {
	message            string
	reason             string
	code_sample        string
	possible_solutions string
	filename           string
	err_type           SG_ERROR
	linenum            int
	linedepth          int
}

func CreateError(node sage.ParseNode, err SG_ERROR, msg string, reason string, solutions string) *SageError {
	linenum := node.Get_token().Linenum
	linedepth := node.Get_token().Linedepth
	sample := node.Get_token().Lexeme
	filename := node.Get_token().Filename

	return &SageError{
		message:            msg,
		reason:             reason,
		code_sample:        sample,
		possible_solutions: solutions,
		linenum:            linenum,
		linedepth:          linedepth,
		filename:           filename,
	}
}

func (e *SageError) RaiseError() {
	var builder strings.Builder
	error_title := err_type_to_title(e.err_type)

	builder.WriteString(fmt.Sprintf("error(%s): %s\n", error_title, e.message))
	builder.WriteString(fmt.Sprintf("   --> %s:%d:%d\n", e.filename, e.linenum, e.linedepth))

}
