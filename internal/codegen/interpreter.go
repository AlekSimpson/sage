package sage

import (
	"fmt"
	"sage/internal/parser"
)

type SageInterpreter struct {
	Global_table *SymbolTable
	errors       []*SageError
}

type Value interface{}

type Result interface {
	ResultType() string
	ResultValue() Value
}

func (i *SageInterpreter) interpret(node sage.ParseNode) Result {
	// TODO:
	// BINARY
	// TRINARY
	// UNARY
	// BLOCK
	// FUNCDEF
	// FUNCCALL
	// TYPE
	// STRUCT
	// IF
	// IF_BRANCH
	// ELSE_BRANCH
	// WHILE
	// ASSIGN
	// FOR
	// RANGE
	// VAR_DEC
	// VAR_REF ?

	current_node := node
	interpretting := true
	for interpretting {
		nodetype := current_node.Get_true_nodetype()
		switch nodetype {
		case sage.ASSIGN:
			// assign node:
			//   - left: list node
			//   - right: value
			if current_node.Get_nodetype() != sage.BINARY {
				// it could be that we are interpretting a declaration and initialization combo statement which would come packaged as a trinary node
				error := NewSageError("Expected binary assignment", "TODO", "Interpreter is WIP", "needs implementation lol")
				i.errors = append(i.errors, error)
				interpretting = false
				break
			}

			bin_node := current_node.(*sage.BinaryNode)
			left_side := bin_node.Left
			right_side := bin_node.Right
			right_value := i.interpret(right_side)

			i.Global_table.SetEntry(left_side, right_value)
			return nil

		case sage.COMPILE_TIME_EXECUTE:
			current_node = current_node.Get_child_node()
			continue

		default:
			error := NewSageError("Invalid statement", fmt.Sprintf("Cannot interpret %s node", nodetype.String()), "Node probably isn't supported at this time", "solutions")
			i.errors = append(i.errors, error)
			error.RaiseError()
			return nil
		}
	}

	return nil
}
