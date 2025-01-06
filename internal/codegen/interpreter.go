package sage

import (
	"fmt"
	"sage/internal/parser"
)

type SageInterpreter struct {
	current_scope *Symbol
	errors        []*SageError
}

func NewInterpreter(scope *Symbol) *SageInterpreter {
	return &SageInterpreter{
		current_scope: scope,
		errors:        nil,
	}
}

func (i *SageInterpreter) interpret(node sage.ParseNode) *AtomicValue {
	// TODO:
	// BINARY
	// TRINARY
	// UNARY
	// BLOCK
	// FUNCDEF
	// FUNCCALL
	// STRUCT
	// IF
	// WHILE
	// ASSIGN
	// FOR
	// RANGE
	// VAR_DEC
	// VAR_REF

	current_node := node
	interpretting := true
	for interpretting {
		nodetype := current_node.Get_true_nodetype()
		switch nodetype {
		case sage.STRUCT:

		case sage.ASSIGN:
			// assign node:
			//   - left: list node
			//   - right: value
			if nodetype == sage.TRINARY {
				i.interpret_trinary_assign(current_node.(*sage.TrinaryNode))
				return nil
			}

			bin_node := current_node.(*sage.BinaryNode)

			left_side := bin_node.Left
			right_side := bin_node.Right
			right_value := i.interpret(right_side)
			if right_value == nil {
				return nil
			}

			if left_side.Get_true_nodetype() == sage.LIST {
				// if its a list then we are accessing a struct
				i.interpret_struct_field_assign(left_side.(*sage.ListNode), right_value.result_value)
				return nil
			}

			symbol, code_output := i.current_scope.scope.LookupSymbol(left_side.Get_token().Lexeme)
			if code_output == NAME_UNDEFINED {
				return nil
			}

			symbol.SetValue(right_value.datatype, right_value.result_value)

			return nil

		case sage.COMPILE_TIME_EXECUTE:
			current_node = current_node.Get_child_node()
			continue

		case sage.STRING:
			return &AtomicValue{ARRAY_CHAR, node.Get_token().Lexeme, nil}
		case sage.NUMBER:
			return &AtomicValue{I32, node.Get_token().Lexeme, nil}
		case sage.FLOAT:
			return &AtomicValue{F32, node.Get_token().Lexeme, nil}

		default:
			fmt.Printf("Cannot interpret node: %s\n", node.String())
			return nil
		}
	}

	return nil
}

func (i *SageInterpreter) interpret_trinary_assign(node *sage.TrinaryNode) {
	variable_name := node.Left.Get_token().Lexeme
	variable_type, failed := resolve_node_type(node)
	if failed {
		return
	}

	output_code := i.current_scope.scope.AddSymbol(variable_name, VARIABLE, nil, variable_type)
	if output_code == NAME_COLLISION {
		return
	}

	symbol, _ := i.current_scope.scope.LookupSymbol(variable_name)
	right_value := *i.interpret(node.Right)
	symbol.SetValue(right_value.datatype, right_value.result_value)
}

func (i *SageInterpreter) interpret_struct_field_assign(node *sage.ListNode, right_side_value any) {
	field_accessors := node.Lexemes
	curr_scope := i.current_scope
	for _, accessor := range field_accessors {
		symbol, code_output := curr_scope.scope.LookupSymbol(accessor)
		if code_output == NAME_UNDEFINED {
			return
		}

		curr_scope = symbol
		if curr_scope.scope == nil {
			break
		}
	}

	last_accessor := field_accessors[len(field_accessors)-1]
	curr_scope.FieldWrite(last_accessor, right_side_value)
}
