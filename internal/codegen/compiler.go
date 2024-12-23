package sage

import (
	"fmt"
	"sage/internal/parser"
)

type SageCompiler struct {
	interpreter  *SageInterpreter
	global_table *SymbolTable
	root_module  *IRModule
}

func BeginCodeCompilation(code_content []byte, filename string) {
	parser := sage.NewParser(code_content)
	parsetree := parser.Parse_program()

	// initialize symbol table structure so we can read and write to it before (technically during) compilation to begin compilation
	build_settings := DefaultBuildSettings(filename)
	GLOBAL_SYMBOL_TABLE := NewSymbolTable()
	GLOBAL_SYMBOL_TABLE.NewEntry("build_settings")
	build_settings_entry := GLOBAL_SYMBOL_TABLE.GetEntryNamed("build_settings")
	build_settings.initialize_symbol_table_entry(build_settings_entry)
	GLOBAL_SYMBOL_TABLE.Insert("build_settings", build_settings_entry)

	// Create IR module from parse tree
	code_module := NewIRModule(filename, GLOBAL_SYMBOL_TABLE)
	code_module.GLOBAL_TABLE = GLOBAL_SYMBOL_TABLE

	interpreter := NewInterpreter(code_module.GLOBAL_TABLE)

	compiler := SageCompiler{interpreter, GLOBAL_SYMBOL_TABLE, code_module}
	compiler.compile_code(parsetree, code_module)
}

func (c *SageCompiler) compile_code(parsetree sage.ParseNode, ir_module *IRModule) {

	var function_defs []IRFunc
	var function_decs []IRFunc
	var globals []IRGlobal
	var structs []IRStruct

	// loop through root node chilren
	// generate IR nodes for each child node
	program_root_node := parsetree.(*sage.BlockNode)
	for _, node := range program_root_node.Children {
		_, ir, ir_type := c.compile(node)
		switch ir_type {
		case "fdefs":
			converted := ir.(*IRFunc)
			function_defs = append(function_defs, *converted)
		case "fdecs":
			converted := ir.(*IRFunc)
			function_decs = append(function_decs, *converted)
		case "globals":
			converted := ir.(*IRGlobal)
			globals = append(globals, *converted)
		case "structs":
			converted := ir.(*IRStruct)
			structs = append(structs, *converted)
		}
	}
}

// NOTE: in the future we may want to expand to returning four return types, with the fourth be a dedicated compile error type
func (c *SageCompiler) compile(node sage.ParseNode) (InstructionList, IRInstructionProtocol, string) {
	switch node.Get_true_nodetype() {
	case sage.BINARY:
		// NEED
	case sage.TRINARY:
		// NEED
	case sage.UNARY:
	case sage.NUMBER:
		// NEED
	case sage.STRING:
		// NEED
	case sage.IDENTIFIER:
	case sage.KEYWORD:
	case sage.BLOCK:
	case sage.PARAM_LIST:
		block := node.(*sage.BlockNode)
		var params []IRAtom
		for _, childnode := range block.Children {
			_, raw, _ := c.compile(childnode)
			param := raw.(*IRAtom)
			params = append(params, *param)
		}
		return params, nil, "nothing"

	case sage.FUNCDEF:
		// all function defs are represented as a binary node in the parse treee
		binary := node.(*sage.BinaryNode)
		function_signature := binary.Right.(*sage.TrinaryNode)

		function_name := binary.Left.Get_token().Lexeme
		function_return_type := function_signature.Middle.Get_token().Lexeme

		// update symbol table
		success := c.global_table.NewEntry(function_name)
		if !success {
			return nil, nil, fmt.Sprintf("Invalid redefinition of function %s\n", function_name)
		}

		raw, _, _ := c.compile(function_signature.Left)
		function_params := raw.([]IRAtom)

		raw, _, _ = c.compile(function_signature.Right)
		function_body := raw.([]IRBlock)

		// generate ir
		ir := &IRFunc{
			name:         function_name,
			return_type:  sage_type_to_llvm_type(function_return_type),
			parameters:   function_params,
			calling_conv: "NOCONV",
			attribute:    "NOATTRIBUTE",
			body:         function_body,
			is_main:      false,
		}

		return nil, ir, "fdefs"

	case sage.FUNCCALL:
		// NEED
	case sage.TYPE:
	case sage.STRUCT:
	case sage.IF:
	case sage.IF_BRANCH:
	case sage.ELSE_BRANCH:
	case sage.WHILE:
	case sage.ASSIGN:
		// NEED
	case sage.FOR:
	case sage.PROGRAM:
	case sage.RANGE:
	case sage.VAR_DEC:
		// NEED
	case sage.VAR_REF:
	case sage.COMPILE_TIME_EXECUTE:
		unary_node := node.(*sage.UnaryNode)
		block_node := unary_node.Get_child_node().(*sage.BlockNode)
		for _, childnode := range block_node.Children {
			c.interpreter.interpret(childnode)
			// NOTE: in the future we may want or need to expand this out but for now all we really need compile time execute to do is update the symbol table
		}
	}

	return nil, nil, "COMPILATION ERROR"
}

func sage_type_to_llvm_type(typestr string) string {
	return ""
}
