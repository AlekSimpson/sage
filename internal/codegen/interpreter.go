package sage

import (
	"fmt"
	"sage/internal/parser"
)

type SageInterpreter struct {
	Global_table *SymbolTable
	errors       []*SageError
}

func (i *SageInterpreter) interpret(node sage.ParseNode) string {
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
				error := NewSageError("Expected binary assignment", "TODO", "Interpreter is WIP", "needs implementation lol")
				i.errors = append(i.errors, error)
				interpretting = false
				break
			}

			bin_node := current_node.(*sage.BinaryNode)
			left_side := bin_node.Left
			right_side := bin_node.Right
			right_value := i.interpret(right_side)

			i.Global_table.SetEntry(left_side.Get_token().Lexeme, right_value)
			return ""

		case sage.COMPILE_TIME_EXECUTE:
			current_node = current_node.Get_child_node()
			continue

		default:
			error := NewSageError("Invalid statement", fmt.Sprintf("Cannot interpret %s node", nodetype.String()), "Node probably isn't supported at this time", "solutions")
			i.errors = append(i.errors, error)
			error.RaiseError()
			return ""
		}
	}

	return ""
}

func (i *SageInterpreter) retrieve_lhs_table_entry(table *SymbolTable, lhs sage.ParseNode, depth int) SymbolTableEntry {
	if lhs.Get_nodetype() == sage.LIST {
		// retrieve structure field
		list_node := lhs.(*sage.ListNode)

		struct_scope_table := table.GetEntry(list_node.Lexemes[depth])
		if struct_scope_table.erroneous_entry {
			return NewEntryError()
		}

		return i.retrieve_lhs_table_entry(struct_scope_table.entry_table, lhs, depth+1)
	}

	return table.GetEntry(lhs.Get_token().Lexeme)
}
