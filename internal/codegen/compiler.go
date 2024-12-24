package sage

import (
	"fmt"
	"sage/internal/parser"
)

type SageCompiler struct {
	interpreter  *SageInterpreter
	global_table *SymbolTable // used to keep track of all values and functions in the program, holds info such as literal values, type info and register associations
	root_module  *IRModule    // used to emit IR once done compiling
	visitor      NodeVisitor  // used to keep track of code semantics in order to emit IR with correct syntax, etc.
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

	compiler := SageCompiler{interpreter, GLOBAL_SYMBOL_TABLE, code_module, nil}
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
	case sage.UNARY:
	case sage.NUMBER:
		unary := node.(*sage.UnaryNode)
		value := unary.Get_token().Lexeme
		inst_type, last_param := decide_instruction_type_and_last_param(c.visitor)
		success := c.global_table.NewEntry(value)
		var entry *SymbolTableEntry
		if success {
			entry = c.global_table.GetEntryNamed(value)
		}

		ir := &IRAtom{
			name:             "number",
			irtype:           sage_type_to_llvm_type("number"),
			value:            value,
			instruction_type: inst_type,
			is_last_param:    last_param,
			table:            entry,
		}

		return nil, ir, "nothing"

	case sage.STRING:
		unary := node.(*sage.UnaryNode)
		value := unary.Get_token().Lexeme
		inst_type, last_param := decide_instruction_type_and_last_param(c.visitor)
		success := c.global_table.NewEntry(value)
		var entry *SymbolTableEntry
		if success {
			entry = c.global_table.GetEntryNamed(value)
		}

		ir := &IRAtom{
			name: "string_literal",
			// NOTE: the substring '_<|>_' is purposely verbose so that it doesn't accidentally get
			// used inside the actual string value itself
			irtype:           sage_type_to_llvm_type(fmt.Sprintf("array_<|>_char_<|>_%s", value)),
			value:            value,
			instruction_type: inst_type,
			is_last_param:    last_param,
			table:            entry,
		}

		return nil, ir, "nothing"

	case sage.IDENTIFIER:
	case sage.KEYWORD:
	case sage.BLOCK:
	case sage.PARAM_LIST:
		block := node.(*sage.BlockNode)
		var params []IRAtom
		for index, childnode := range block.Children {
			c.visitor.(*FuncDefVisitor).Current_param = index
			_, raw, _ := c.compile(childnode)
			param := raw.(*IRAtom)
			params = append(params, *param)
		}
		return params, nil, "nothing"

	case sage.FUNCDEF:
		// all function defs are represented as a binary node in the parse treee
		binary := node.(*sage.BinaryNode)
		function_signature := binary.Right.(*sage.TrinaryNode)
		param_node := function_signature.Right.(*sage.BlockNode)
		c.visitor = &FuncDefVisitor{
			Parameter_amount: len(param_node.Children),
			Current_param:    0,
		}

		function_name := binary.Left.Get_token().Lexeme
		function_return_type := function_signature.Middle.Get_token().Lexeme

		// update symbol table
		success := c.global_table.NewEntry(function_name)
		if !success {
			return nil, nil, fmt.Sprintf("Invalid redefinition of function %s\n", function_name)
		}
		c.global_table.InsertValue(function_name, c.visitor)

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
		}

		return nil, ir, "fdefs"

	case sage.FUNCCALL:
		c.visitor = &FuncCallVisitor{}
		unary := node.(*sage.UnaryNode)
		function_name := unary.Get_token().Lexeme

		// get parameter block node to update visitor
		parameters_raw := unary.Get_child_node() // should return BlockNode
		parameters := parameters_raw.(*sage.BlockNode)
		c.visitor = &FuncDefVisitor{
			Parameter_amount: len(parameters.Children),
			Current_param:    0,
		}

		// get []IRAtom struct list from parameters
		conv, _, _ := c.compile(parameters_raw)
		ir_parameters := conv.([]IRAtom)

		// get the function's return type
		global_table_result := c.global_table.GetEntryNamed(function_name)

		ir := &IRFuncCall{
			tail:         false,
			calling_conv: "NOCONV",
			return_type:  sage_type_to_llvm_type(global_table_result.entry_value.ResultType()),
			name:         function_name,
			parameters:   ir_parameters,
			table:        global_table_result,
		}

		return nil, ir, "nothing"

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
		fmt.Println("TODO: PROGRAM not implemented yet (might not need)")
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
	// TODO: our sage_type_to_llvm_type will probably have to handle the generation of these array types
	// func (ir *IRStringLiteral) ToLLVM() string {
	// 	return fmt.Sprintf("[%d x %s] c\"%s\"", len(ir.literal_content), ir.irtype, ir.literal_content)
	// }
	return ""
}

func decide_instruction_type_and_last_param(visitor NodeVisitor) (VarInstructionType, bool) {
	switch visitor.CurrentlyVisiting() {
	case FUNCCALL:
		func_call_visitor := visitor.(*FuncCallVisitor)
		return PARAM, func_call_visitor.Current_param == func_call_visitor.Parameter_amount-1
		// TODO: more VarInstructionTypes supported
	}

	return -1, false
}
