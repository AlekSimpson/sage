package sage

import (
	"fmt"
	"os"
	"sage/internal/parser"
	"strings"
)

type SageCompiler struct {
	interpreter *SageInterpreter
	root_module *IRModule    // used to emit IR once done compiling
	visitor     *NodeVisitor // used to keep track of code semantics in order to emit IR with correct syntax, etc.
	// TODO: create Logger object
}

func BeginCodeCompilation(code_content []byte, filename string) {
	parser := sage.NewParser(code_content)
	parsetree := parser.Parse_program()

	// initialize symbol table structure so we can read and write to it before (technically during) compilation to begin compilation
	build_settings := DefaultBuildSettings(filename)
	GLOBAL_SYMBOL_TABLE := NewSymbolTable()
	build_settings_entry := GLOBAL_SYMBOL_TABLE.NewEntry("build_settings")
	build_settings.initialize_symbol_table_entry(build_settings_entry)

	// Create IR module from parse tree
	code_module := NewIRModule(filename, GLOBAL_SYMBOL_TABLE)
	code_module.GLOBAL_TABLE = GLOBAL_SYMBOL_TABLE

	interpreter := NewInterpreter(code_module.GLOBAL_TABLE)

	compiler := SageCompiler{interpreter, code_module, NewRootVisitor(GLOBAL_SYMBOL_TABLE)}
	compiler.compile_code(parsetree, code_module)
}

func (c *SageCompiler) compile_code(parsetree sage.ParseNode, ir_module *IRModule) {
	function_defs := []IRFunc{}
	function_decs := []IRFunc{}
	globals := []IRInstructionProtocol{}
	structs := []IRStruct{}

	// loop through root node chilren
	// generate IR nodes for each child node
	program_root_node := parsetree.(*sage.BlockNode)
	for _, node := range program_root_node.Children {
		ir, code_category := c.compile_general_node(node)
		switch code_category {
		case "fdefs":
			ir_func := ir.(IRFunc)
			function_defs = append(function_defs, ir_func)

		case "fdecs":
			ir_func := ir.(IRFunc)
			function_decs = append(function_decs, ir_func)

		case "structs":
			ir_struct := ir.(IRStruct)
			structs = append(structs, ir_struct)

		case "globals":
			ir_global := ir.(IRAtom)
			ir_global.instruction_type = GLOBAL
			globals = append(globals, ir_global)

		case "includes":
			// include IR does not require instruction_type being set to GLOBAL
			// so it is in its own case to keep it seperate
			globals = append(globals, ir)

		case "compile_time":
			// fmt.Println("Interpretting run directive...")
			break

		default:
			// otherwise an error was thrown
			fmt.Println(code_category)
		}
	}

	ir_module.FuncDefs = function_defs
	ir_module.FuncDecs = function_decs
	ir_module.Globals = globals
	ir_module.Structs = structs
}

func (c *SageCompiler) compile_general_node(node sage.ParseNode) (IRInstructionProtocol, string) {
	switch node.Get_true_nodetype() {
	case sage.FUNCDEF:
		ir, err_msg := c.compile_funcdef_node(node.(*sage.BinaryNode))
		if err_msg != "nothing" {
			return nil, err_msg
		}

		if len(ir.body) == 0 {
			// if the body is empty then it is a function declaration
			return ir, "fdecs"
		}

		return ir, "fdefs"
	case sage.VAR_DEC:
		ir, err_msg := c.compile_var_dec_node(node.(*sage.BinaryNode))
		if err_msg != "nothing" {
			return nil, err_msg
		}

		return ir, "globals"

	case sage.STRUCT:
		ir, err_msg := c.compile_struct_node(node)
		if err_msg != "nothing" {
			return nil, err_msg
		}

		return ir, "structs"

	case sage.INCLUDE:
		ir, err_msg := c.compile_include_node(node.(*sage.UnaryNode))
		if err_msg != "nothing" {
			return nil, err_msg
		}

		return ir, "includes"

	case sage.COMPILE_TIME_EXECUTE:
		// NOTE: shouldn't we also be getting error logs from this as well?
		c.compile_compile_time_execute_node(node.(*sage.UnaryNode))
		return nil, "compile_time"

	default:
		return nil, fmt.Sprintf("COMPILATION ERROR: Cannot have %s in top level scope.", node.Get_true_nodetype())
	}
}

func (c *SageCompiler) compile_include_node(node *sage.UnaryNode) (IRInclude, string) {
	include_file := node.Get_token().Lexeme

	// get rid of the '"' symbols wrapping the module name
	include_file = include_file[1 : len(include_file)-1]

	search_path := "/home/alek/Desktop/projects/sage/"
	content, err := os.ReadFile(search_path + include_file)
	if err != nil {
		return IRInclude{}, fmt.Sprintf("COMPILATION ERROR: %s", err.Error())
	}

	ir := IRInclude{
		module_name: include_file,
		contents:    string(content),
	}

	return ir, "nothing"
}

func (c *SageCompiler) compile_binary_node(node *sage.BinaryNode, first_entry bool) ([]IRInstructionProtocol, string) {
	// compiles expression node which for now only deals with numeric values,
	// but could include terms that have func calls that return numeric values

	op_lexeme := node.Get_token().Lexeme
	if first_entry {
		expression_result_type, error := resolve_expression_type(node)
		if error {
			// if error is true then resolveGLOBAL_expression_type will store the error message in expression_result_type
			return nil, expression_result_type
		}

		// get visitor and symbol table information
		c.visitor = NewVisitor(EXPRESSION, op_lexeme, c.visitor)
		c.visitor.result_type = expression_result_type
	}

	var total_expression_ir []IRInstructionProtocol

	// evalute both branches of nodes to extract operands and or generate any needed extra IR instructions
	leftside_ir, operand1, successful := c.visit_operand_branch(node.Left)
	if !successful {
		// in cases non-successful cases operand2 stores the error message string
		return leftside_ir, operand1
	}
	total_expression_ir = append(total_expression_ir, leftside_ir...)

	rightside_ir, operand2, successful := c.visit_operand_branch(node.Right)
	if !successful {
		// in cases non-successful cases operand2 stores the error message string
		return rightside_ir, operand2
	}
	total_expression_ir = append(total_expression_ir, rightside_ir...)

	// now generate the most current IR instruction
	op_name := op_lexeme_to_llvm_op_lexeme(c.visitor, node.Get_token().Lexeme)
	ir := &IRExpression{
		operation_name: op_name,
		irtype:         c.visitor.result_type,
		operand1:       operand1,
		operand2:       operand2,
		table:          c.visitor.table,
	}

	total_expression_ir = append(total_expression_ir, ir)

	if first_entry {
		c.visitor = c.visitor.parent_visitor
	}

	return total_expression_ir, "nothing"
}

func (c *SageCompiler) compile_expression_operand(node sage.ParseNode) ([]IRInstructionProtocol, string) {
	switch node.Get_true_nodetype() {
	case sage.BINARY:
		return c.compile_binary_node(node.(*sage.BinaryNode), false)
	case sage.FUNCCALL:
		call_ir, err_msg := c.compile_func_call_node(node.(*sage.UnaryNode))
		return []IRInstructionProtocol{call_ir}, err_msg
	case sage.VAR_REF:
		ref_ir, err_msg := c.compile_var_ref_node(node.(*sage.UnaryNode))
		return []IRInstructionProtocol{ref_ir}, err_msg
	}

	return []IRInstructionProtocol{}, "COMPILATION ERROR: Invalid operand found in expression\n"
}

func (c *SageCompiler) visit_operand_branch(branch_node sage.ParseNode) (ir []IRInstructionProtocol, operand string, successful bool) {
	branchnode_type := branch_node.Get_true_nodetype()
	if branchnode_type == sage.NUMBER || branchnode_type == sage.FLOAT {
		operand2 := branch_node.Get_token().Lexeme
		return []IRInstructionProtocol{}, operand2, true
	}

	branch_ir, err_msg := c.compile_expression_operand(branch_node)
	if err_msg != "nothing" {
		return branch_ir, err_msg, false
	}

	// MARK
	entry, _ := c.visitor.table.GetEntryNamed(c.visitor.scope_name)
	operand2 := entry.registers.Pop().Register
	return branch_ir, operand2, true
}

// func (c *SageCompiler) compile_unary_node(node *sage.UnaryNode) ([]IRInstructionProtocol, string) {}

func (c *SageCompiler) compile_string_node(node *sage.UnaryNode) (*IRAtom, string) {
	value := node.Get_token().Lexeme
	inst_type, last_param := decide_instruction_type_and_last_param(c.visitor)

	// MARK
	entry := c.visitor.table.NewEntry(value)
	if entry == nil {
		return &IRAtom{}, "COMPILATION ERROR: Unable to find string node entry"
	}

	ir := &IRAtom{
		name: "string_literal",
		// NOTE: the substring '_<|>_' is purposely verbose so that it doesn't accidentally get
		// used inside the actual string value itself
		irtype:           sage_type_to_llvm_type(fmt.Sprintf("array_<|>_char_<|>_%d", len(value))),
		value:            value,
		instruction_type: inst_type,
		is_last_param:    last_param,
		table:            c.visitor.table,
	}

	if c.visitor.scope_name == "ROOT" {
		return ir, "globals"
	}

	return ir, "nothing"
}

func (c *SageCompiler) compile_keyword_node(node *sage.UnaryNode) ([]IRInstructionProtocol, string) {
	// NOTE: this function assumes the keyword is "return", the future more keyword statements could exist and checks would be needed for those
	var return_instructions []IRInstructionProtocol

	if node.Get_child_node() != nil {
		// if a child node is present then that means we are returning a value, so we need to compile the IR required for that as well
		return_node := node.Get_child_node()
		switch return_node.Get_true_nodetype() {
		case sage.STRING:
			literal_ir, err_msg := c.compile_string_node(return_node.(*sage.UnaryNode))
			if err_msg != "nothing" {
				return []IRInstructionProtocol{literal_ir}, err_msg
			}

			return_instructions = append(return_instructions, literal_ir)

		case sage.VAR_REF:
			ref_ir, err_msg := c.compile_var_ref_node(return_node.(*sage.UnaryNode))
			if err_msg != "nothing" {
				return []IRInstructionProtocol{ref_ir}, err_msg
			}

			return_instructions = append(return_instructions, ref_ir)

		case sage.NUMBER: // if its a number then it will just compile the same as float would
		case sage.FLOAT:
			raw_value := node.Get_child_node().Get_token().Lexeme

			return_ir := &IRAtom{
				irtype:           c.visitor.result_type,
				value:            raw_value,
				instruction_type: RET_STMT,
			}
			return_instructions = append(return_instructions, return_ir)

			return return_instructions, ""

		case sage.FUNCCALL:
			call_ir, err_msg := c.compile_func_call_node(return_node.(*sage.UnaryNode))
			if err_msg != "nothing" {
				return []IRInstructionProtocol{call_ir}, err_msg
			}

			return_instructions = append(return_instructions, call_ir)

		case sage.BINARY: // expression
			expression_ir, err_msg := c.compile_binary_node(return_node.(*sage.BinaryNode), true)
			if err_msg != "nothing" {
				return expression_ir, err_msg
			}

			return_instructions = append(return_instructions, expression_ir...)
		}

		// MARK
		return_ir := &IRAtom{
			irtype:           c.visitor.result_type,
			value:            c.visitor.table.registers.Pop().Register,
			instruction_type: RET_STMT,
		}
		return_instructions = append(return_instructions, return_ir)

		return return_instructions, ""
	}

	// irtype and value arent needed here because if the return has nothing attached then it will always be returning void
	return_ir := &IRAtom{
		instruction_type: RET_STMT,
	}

	return_instructions = append(return_instructions, return_ir)

	return return_instructions, ""
}

func (c *SageCompiler) compile_block_node(node *sage.BlockNode, block_label string) ([]IRBlock, string) {
	var blocks []IRBlock
	var block_contents []IRInstructionProtocol

	for _, childnode := range node.Children {
		switch childnode.Get_true_nodetype() {
		case sage.KEYWORD:
			keyword_ir, err_msg := c.compile_keyword_node(childnode.(*sage.UnaryNode))
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, keyword_ir...)

		case sage.FUNCDEF:
			def_ir, err_msg := c.compile_funcdef_node(childnode.(*sage.BinaryNode))
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, def_ir)

		case sage.FUNCCALL:
			call_ir, err_msg := c.compile_func_call_node(childnode.(*sage.UnaryNode))
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, call_ir)

		case sage.IF:
			conditional_ir, err_msg := c.compile_if_node(childnode)
			if err_msg != "nothing" {
				return blocks, "nothing"
			}

			blocks = append(blocks, conditional_ir.condition_checks...)
			blocks = append(blocks, conditional_ir.condition_bodies...)
			if conditional_ir.else_body != nil {
				blocks = append(blocks, *conditional_ir.else_body)
			}

		case sage.WHILE:
			while_ir, err_msg := c.compile_while_node(childnode)
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			blocks = append(blocks, while_ir.entry)
			blocks = append(blocks, while_ir.condition)
			blocks = append(blocks, while_ir.body)
			blocks = append(blocks, while_ir.end)

		case sage.FOR:
			for_ir, err_msg := c.compile_for_node(childnode)
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			blocks = append(blocks, for_ir.entry)
			blocks = append(blocks, for_ir.condition)
			blocks = append(blocks, for_ir.body)
			blocks = append(blocks, for_ir.increment)
			blocks = append(blocks, for_ir.end)

		case sage.ASSIGN:
			assign_ir, err_msg := c.compile_assign_node(childnode)
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, assign_ir...)

		case sage.STRUCT:
			struct_ir, err_msg := c.compile_struct_node(childnode)
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, struct_ir)

		case sage.VAR_DEC:
			dec_ir, err_msg := c.compile_var_dec_node(childnode.(*sage.BinaryNode))
			if err_msg != "nothing" {
				return blocks, err_msg
			}

			block_contents = append(block_contents, dec_ir)

		case sage.COMPILE_TIME_EXECUTE:
			c.compile_compile_time_execute_node(childnode.(*sage.UnaryNode))

		default:
			return blocks, "COMPILATION ERROR: Found statement that does not make sense within context of a block\n"
		}

		main_block := &IRBlock{
			Name: block_label,
			Body: block_contents,
		}

		blocks = append(blocks, *main_block)
	}

	return blocks, "nothing"
}

func (c *SageCompiler) compile_param_list_node(node *sage.BlockNode) ([]IRAtom, string) {
	var params []IRAtom
	for index, childnode := range node.Children {
		c.visitor.current_param = index

		// all children inside a funcdef param list will be a BinaryNode
		param := IRAtom{
			name:   childnode.(*sage.BinaryNode).Left.Get_token().Lexeme,
			irtype: sage_type_to_llvm_type(childnode.(*sage.BinaryNode).Right.Get_token().Lexeme),
		}
		params = append(params, param)
	}

	return params, "nothing"
}

func (c *SageCompiler) compile_funcdef_node(node *sage.BinaryNode) (IRFunc, string) {
	// save the parent visitor so that we can restore it when we are finished visiting the child
	previous_visitor := c.visitor

	// all function defs are represented as a binary node in the parse treee
	function_signature := node.Right.(*sage.TrinaryNode)
	param_node := function_signature.Right.(*sage.BlockNode)
	function_name := node.Left.Get_token().Lexeme
	parameters := function_signature.Left.(*sage.BlockNode)
	body := function_signature.Right.(*sage.BlockNode)
	c.visitor = &NodeVisitor{
		scope_name:       function_name,
		parameter_amount: len(param_node.Children),
		current_param:    0,
		visit_type:       FUNCDEF,
	}
	function_return_type := function_signature.Middle.Get_token().Lexeme

	// FIX: update symbol table
	// entry := c.visitor.table.NewEntry(function_name)
	// if entry == nil {
	// 	return IRFunc{}, fmt.Sprintf("Invalid redefinition of function %s\n", function_name)
	// }
	// c.global_table.InsertValue(function_name, c.visitor)

	function_params, err_msg := c.compile_param_list_node(parameters)
	if err_msg != "nothing" {
		return IRFunc{}, err_msg
	}

	// the body could be omitted if it is simply a function declaration and not a definition
	var function_body []IRBlock
	if body != nil {
		function_body, err_msg = c.compile_block_node(body, "entry")
		if err_msg != "nothing" {
			return IRFunc{}, err_msg
		}
	}

	// generate ir
	ir := IRFunc{
		name:         function_name,
		return_type:  sage_type_to_llvm_type(function_return_type),
		parameters:   function_params,
		calling_conv: "NOCONV",
		attribute:    "NOATTRIBUTE",
		body:         function_body,
	}

	c.visitor = previous_visitor

	return ir, "nothing"
}

func (c *SageCompiler) compile_func_call_node(node *sage.UnaryNode) (IRFuncCall, string) {
	function_name := node.Get_token().Lexeme

	// save the parent visitor so that we can restore it when we are done visitin the child
	previous_visitor := c.visitor

	// get parameter block node to update visitor
	parameters_raw := node.Get_child_node() // should return BlockNode
	parameters := parameters_raw.(*sage.BlockNode)
	c.visitor = &NodeVisitor{
		scope_name:       function_name,
		parameter_amount: len(parameters.Children),
		current_param:    0,
		visit_type:       FUNCCALL,
	}

	// get []IRAtom struct list from parameters
	var ir_parameters []IRInstructionProtocol
	for _, parameter := range parameters.Children {
		// there is more freedom on what values can be passed into a function, theoretically any kind of primary or expression could be passed as a parameter
		param, err_msg := c.compile_primary_node(parameter)
		if err_msg != "nothing" {
			return IRFuncCall{}, err_msg
		}

		ir_parameters = append(ir_parameters, param...)
	}

	// get the function's return type
	global_table_result := c.global_table.GetEntryNamed(function_name)

	ir := IRFuncCall{
		tail:         false,
		calling_conv: "NOCONV",
		return_type:  sage_type_to_llvm_type(global_table_result.entry_value.ResultType()),
		name:         function_name,
		parameters:   ir_parameters,
		table:        global_table_result,
	}

	c.visitor = previous_visitor

	return ir, "nothing"
}

func (c *SageCompiler) compile_struct_node(node sage.ParseNode) (IRStruct, string) {
	return IRStruct{}, ""
}

func (c *SageCompiler) compile_if_node(node sage.ParseNode) (IRConditional, string) {
	return IRConditional{}, ""
}

func (c *SageCompiler) compile_while_node(node sage.ParseNode) (IRWhile, string) {
	return IRWhile{}, ""
}

func (c *SageCompiler) compile_assign_node(node sage.ParseNode) ([]IRInstructionProtocol, string) {
	var retval []IRInstructionProtocol
	if node.Get_nodetype() == sage.BINARY {
		binary := node.(*sage.BinaryNode)
		store_ident := binary.Left.Get_token().Lexeme

		rhs_ir, err_msg := c.compile_primary_node(binary.Right)
		if err_msg != "nothing" {
			return retval, err_msg
		}

		retval = append(retval, rhs_ir...)

		variable_entry := c.global_table.GetEntryNamed(store_ident)
		statement_type, error := resolve_expression_type(binary.Right)
		if error {
			return retval, fmt.Sprintf("COMPILATION ERROR: Could not resolve the type of node %s\n", binary.Right.String())
		}
		if statement_type != variable_entry.entry_value.ResultType() {
			return retval, fmt.Sprintf("COMPILATION ERROR: Type mismatch found, cannot update %s type to %s type.\n", statement_type, variable_entry.entry_value.ResultType())
		}

		store_instruction := IRAtom{
			name:             store_ident,
			irtype:           statement_type,
			instruction_type: STORE,
			table:            variable_entry,
		}

		retval = append(retval, store_instruction)
		return retval, "nothing"

	} else if node.Get_nodetype() == sage.TRINARY {
		trinary := node.(*sage.TrinaryNode)
		name_to_assign_to := trinary.Left.Get_token().Lexeme

		// first evaluate the rhs and create ir for the rhs
		rhs_instruction, err_msg := c.compile_primary_node(trinary.Right)
		if err_msg != "nothing" {
			return retval, err_msg
		}

		rhs_type, error := resolve_expression_type(trinary.Right)
		if error {
			return retval, "COMPILATION ERROR: Was unable to resolve type of rhs expression in assign statement\n"
		}

		retval = append(retval, rhs_instruction...)

		// NOTE: might be a faulty assumption to assume that we should be looking at the global scope here
		success := c.global_table.NewEntry(name_to_assign_to)
		if !success {
			return retval, fmt.Sprintf("COMPILATION ERROR: Invalid redefinition of variable %s\n", name_to_assign_to)
		}
		c.global_table.InsertValue(name_to_assign_to, c.visitor)

		// then create the ir for the lhs (alloca and store)
		assignment_type := sage_type_to_llvm_type(trinary.Middle.Get_token().Lexeme)
		if assignment_type != rhs_type {
			return retval, fmt.Sprintf("COMPILATION ERROR: Type mismatch found, cannot assign %s type to %s type.\n", assignment_type, rhs_type)
		}

		stack_allocate := IRAtom{
			name:             name_to_assign_to,
			irtype:           assignment_type,
			instruction_type: INIT,
			table:            c.global_table.GetEntryNamed(c.visitor.scope_name),
		}
		retval = append(retval, stack_allocate)

		store_instruction := IRAtom{
			name:             name_to_assign_to,
			irtype:           assignment_type,
			instruction_type: STORE,
		}
		retval = append(retval, store_instruction)

		// package all the ir blocks in order into a single list ir block
		return retval, "nothing"
	}

	return retval, fmt.Sprintf("COMPILATION ERROR: Cannot compile assign node: %s.\n", node)
}

func (c *SageCompiler) compile_primary_node(node sage.ParseNode) ([]IRInstructionProtocol, string) {
	truetype := node.Get_true_nodetype()
	retval := []IRInstructionProtocol{}
	switch truetype {
	case sage.STRING:
		ir, str := c.compile_string_node(node.(*sage.UnaryNode))
		retval = append(retval, ir)
		return retval, str

	case sage.FUNCCALL:
		ir, str := c.compile_func_call_node(node.(*sage.UnaryNode))
		retval = append(retval, ir)
		return retval, str

	case sage.VAR_REF:
		ir, str := c.compile_var_ref_node(node.(*sage.UnaryNode))
		retval = append(retval, ir)
		return retval, str

	case sage.BINARY:
		// it is an expression
		ir, str := c.compile_binary_node(node.(*sage.BinaryNode), true)
		retval = append(retval, ir...)
		return retval, str
	default:
		return retval, fmt.Sprintf("COMPILATION ERROR: Could not compile right hand side: %s.\n", node)
	}
}

func (c *SageCompiler) compile_for_node(node sage.ParseNode) (IRFor, string) {
	return IRFor{}, ""
}

// func (c *SageCompiler) compile_range_node(node sage.ParseNode) ([]IRInstructionProtocol, string) {}

func (c *SageCompiler) compile_var_dec_node(node *sage.BinaryNode) (*IRAtom, string) {
	assignment_type := sage_type_to_llvm_type(node.Right.Get_token().Lexeme)
	var_name := node.Left.Get_token().Lexeme

	stack_allocate := IRAtom{
		name:             var_name,
		irtype:           assignment_type,
		instruction_type: INIT,
		table:            c.global_table.GetEntryNamed(c.visitor.scope_name),
	}

	// update symbol table and register tracker with new variable
	current_nest := c.global_table.GetEntryNamed(c.visitor.scope_name)
	current_nest.entry_table.SetEntryNamed(var_name, &AtomicResult{result_type: assignment_type, value: var_name})
	current_nest.NewRegisterFor(var_name)

	return &stack_allocate, "nothing"
}

func (c *SageCompiler) compile_var_ref_node(node *sage.UnaryNode) (*IRAtom, string) {
	var_name := node.Get_token().Lexeme

	// could be inside scoped table, access that table first, should return the global table by default if that is the current scope
	// table := c.global_table.GetEntryNamed(c.visitor.scope_name)
	symbol_table_entry := c.visitor.table.entry_table.GetEntryNamed(var_name)

	load_inst := IRAtom{
		name:             var_name,
		irtype:           symbol_table_entry.entry_value.ResultType(),
		instruction_type: REF,
		table:            c.visitor.table,
	}
	return &load_inst, "nothing"
}

func (c *SageCompiler) compile_compile_time_execute_node(node *sage.UnaryNode) {
	block_node := node.Get_child_node().(*sage.BlockNode)
	for _, childnode := range block_node.Children {
		c.interpreter.interpret(childnode)
		// NOTE: in the future we may want or need to expand this out but for now all we really need compile time execute to do is update the symbol table
	}
}

func sage_type_to_llvm_type(typestr string) string {
	is_pointer := false
	typemap := map[string]string{
		"int":   "i32",
		"float": "float",
		"char":  "i8",
		"void":  "void",
		"bool":  "i1",
		"byte":  "i8",
	}
	retval := ""
	if typestr[len(typestr)-1] == '*' {
		is_pointer = true
		typestr = typestr[:len(typestr)-1]
	}

	retval, exists := typemap[typestr]
	if !exists {
		// otherwise it must be an array
		substrings := strings.Split(typestr, "_<|>_")
		arraytype, exists := typemap[substrings[1]]
		if !exists || arraytype == "void" {
			return fmt.Sprintf("COMPILATION ERROR: Found unknown type: %s.\n", substrings[1])
		}

		retval = fmt.Sprintf("[%s x %s]", substrings[2], arraytype)
	}

	if is_pointer {
		retval += "*" // llvm also represents pointer types by appending a star to the end of the type string
	}

	return retval
}

func decide_instruction_type_and_last_param(visitor *NodeVisitor) (inst_type VarInstructionType, is_last_param bool) {
	switch visitor.visit_type {
	case FUNCCALL:
		return PARAM, visitor.current_param == visitor.parameter_amount-1
		// TODO: more VarInstructionTypes supported
	}

	return -1, false
}

func resolve_expression_type(node sage.ParseNode) (exp_type string, error bool) {
	if node.Get_true_nodetype() == sage.NUMBER {
		return "i32", false
	} else if node.Get_true_nodetype() == sage.FLOAT {
		return "float", false
	} else if node.Get_true_nodetype() == sage.BINARY {
		binary := node.(*sage.BinaryNode)
		op1_type, error := resolve_expression_type(binary.Left)
		if error {
			return op1_type, true
		}

		op2_type, error := resolve_expression_type(binary.Right)
		if error {
			return op2_type, true
		}

		if op1_type == "float" || op2_type == "float" {
			return "float", false
		}

		return "i32", false

	} else {
		return fmt.Sprintf("COMPILATION ERROR: CANNOT RESOLVE TYPE %s\n", node.Get_token().Lexeme), true
	}
}

func op_lexeme_to_llvm_op_lexeme(visitor *NodeVisitor, op_string string) string {
	var div_mnemonic string = "sdiv"
	var mul_mnemonic string = "mul"
	if visitor.result_type == "float" {
		div_mnemonic = "fdiv"
		mul_mnemonic = "fmul"
	}

	op_map := map[string]string{
		"+": "add",
		"-": "sub",
		"/": div_mnemonic,
		"*": mul_mnemonic,
	}

	return op_map[op_string]
}
