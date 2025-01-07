package sage

import (
	"fmt"
	"os"
	"os/exec"
	"sage/internal/parser"
	"strings"
)

type CompilationResult struct {
	module_cat MODULE_CAT
	error      string
}

func NewResult(category MODULE_CAT, err string) *CompilationResult {
	return &CompilationResult{
		module_cat: category,
		error:      err,
	}
}

type MODULE_CAT int

// module categories for the IR module construction
const (
	MOD_FDEF MODULE_CAT = iota
	MOD_FDEC
	MOD_STRUCT
	MOD_GLOBAL
	MOD_INCLUDE
	MOD_COMPILE_TIME
	MOD_ERR
	MOD_NONE
)

type SageCompiler struct {
	interpreter   *SageInterpreter
	ir_module     *IRModule // used to emit IR once done compiling
	current_scope *Symbol
	// TODO: create Logger object
}

func create_build_settings() map[string]any {
	return map[string]any{
		"targetfile":         nil,
		"executable_name":    nil,
		"platform":           nil,
		"architecture":       nil,
		"bitsize":            nil,
		"optimization_level": nil,
		"program_arguments":  nil,
		"argument_count":     nil,
	}
}

const LEXER_DEBUG = false

func BeginCodeCompilation(filename string) {
	del := strings.Split(filename, ".")
	if len(del) != 2 || (del[1] != "g" && del[1] != "sage") {
		panic("Cannot compile inputted file type")
	}

	contents, err := os.ReadFile(filename)
	if err != nil {
		panic(err)
	}

	parser := sage.NewParser(filename, contents)
	parsetree := parser.Parse_program(LEXER_DEBUG)

	if parsetree == nil {
		fmt.Println("Unable to generate parsetree.")
		return
	}

	// initialize symbol table structure so we can read and write to it before (technically during) compilation to begin compilation
	build_settings := create_build_settings()
	symbol_table := &SymbolTable{}
	symbol_table.AddSymbol("build_settings", STRUCT, &AtomicValue{STRUCT_TYPE, nil, build_settings}, STRUCT_TYPE)

	global_scope := &Symbol{
		name:     "GLOBAL",
		ast_type: PROGRAMROOT,
		scope:    symbol_table,
	}

	code_module := NewIRModule(filename)
	interpreter := NewInterpreter(global_scope)

	compiler := SageCompiler{interpreter, code_module, global_scope}
	compiler.compile_code(parsetree, false) // using false because this function is meant to only compile sage project source files

	// TODO: create the .ll file and emit the IR from the ir_module
	//		 might want to create new function for this.
	compiler.emit_ir()
}

func (c *SageCompiler) emit_ir() {
	build_settings, _ := c.current_scope.scope.LookupSymbol("build_settings")
	raw, exists := build_settings.value.struct_map["executable_name"]
	var executable_filename string
	if !exists {
		executable_filename = "a.out"
	} else {
		// if the name does exist then retrieve it and remove the wrapping '"' symbols around the name
		executable_filename = raw.(string)
		executable_filename = executable_filename[1 : len(executable_filename)-1]
	}

	llvm_filename := fmt.Sprintf("%s.ll", executable_filename)
	object_filename := fmt.Sprintf("%s.o", executable_filename)

	// create the IR file with the IR
	llvm_ir := c.ir_module.ToLLVM()
	err := os.WriteFile(llvm_filename, []byte(llvm_ir), 0755)
	if err != nil {
		fmt.Println("Could not write to LLVM file.")
		return
	}

	const TESTING_IR_GENERATION = true
	if TESTING_IR_GENERATION {
		return
	}

	objcmd := exec.Command("llc", "-relocation-model=pic", "-filetype=obj", llvm_filename, fmt.Sprintf("-o %s", object_filename))
	_, err = objcmd.Output()
	if err != nil {
		fmt.Println("Could not create object file.")
		return
	}

	make_executable := exec.Command("gcc", object_filename, fmt.Sprintf("-o %s", executable_filename))
	_, err = make_executable.Output()
	if err != nil {
		fmt.Println("Could not create executable.")
		return
	}

	err = os.Remove(object_filename)
	if err != nil {
		fmt.Println("Could not delete object file.")
		return
	}

	err = os.Remove(llvm_filename)
	if err != nil {
		fmt.Println("Could not delete llvm ir file.")
	}

	// [1] llc -relocation-model=pic -filetype=obj add.ll -o objfile
	// [2] gcc objfile -o exec
}

func (c *SageCompiler) compile_code(parsetree sage.ParseNode, is_module_file bool) {
	function_defs := []IRFunc{}
	function_decs := []IRFunc{}
	globals := []IRInstructionProtocol{}
	structs := []IRStruct{}

	program_root_node := parsetree.(*sage.BlockNode)
	for count, node := range program_root_node.Children {
		fmt.Printf("[%d] compiling node: %s...\n", count, node.String())
		ir, code_category := c.compile_general_node(node)
		switch code_category.module_cat {
		case MOD_FDEF:
			ir_func := ir.(IRFunc)
			function_defs = append(function_defs, ir_func)
			fmt.Println("done.")

		case MOD_FDEC:
			ir_func := ir.(IRFunc)
			function_decs = append(function_decs, ir_func)
			fmt.Println("done.")

		case MOD_STRUCT:
			ir_struct := ir.(IRStruct)
			structs = append(structs, ir_struct)
			fmt.Println("done.")

		case MOD_GLOBAL:
			ir_global := ir.(IRAtom)
			ir_global.instruction_type = GLOBAL
			globals = append(globals, ir_global)
			fmt.Println("done.")

		case MOD_INCLUDE:
			if is_module_file {
				fmt.Println("COMPILATION ERROR:\nInvalid module file found inside included module.\nPlease ensure that all sage files in the module do not contain any `include` statements.")
				return
			}

		case MOD_COMPILE_TIME:
			fmt.Println("done.")

		default:
			// otherwise an error was thrown
			fmt.Println(code_category.error)
		}
	}

	c.ir_module.FuncDefs = append(c.ir_module.FuncDefs, function_defs...)
	c.ir_module.FuncDecs = append(c.ir_module.FuncDecs, function_decs...)
	c.ir_module.Globals = append(c.ir_module.Globals, globals...)
	c.ir_module.Structs = append(c.ir_module.Structs, structs...)
}

// TODO: simplify this function. it is very repetative, could make the cases into a single function thats called in each case with different parameters
func (c *SageCompiler) compile_general_node(node sage.ParseNode) (IRInstructionProtocol, *CompilationResult) {
	switch node.Get_true_nodetype() {
	case sage.FUNCDEF:
		ir, result := c.compile_func_def_node(node.(*sage.BinaryNode))
		if result.module_cat == MOD_ERR {
			return nil, result
		}

		return ir, NewResult(MOD_FDEF, "")
	case sage.FUNCDEC:
		ir, result := c.compile_func_dec_node(node.(*sage.BinaryNode))
		if result.module_cat == MOD_ERR {
			return nil, result
		}

		return ir, NewResult(MOD_FDEC, "")

	case sage.VAR_DEC:
		ir, result := c.compile_var_dec_node(node.(*sage.BinaryNode))
		if result.module_cat == MOD_ERR {
			return nil, result
		}

		return ir, NewResult(MOD_GLOBAL, "")

	case sage.STRUCT:
		ir, result := c.compile_struct_node(node)
		if result.module_cat == MOD_ERR {
			return nil, result
		}

		return ir, NewResult(MOD_STRUCT, "")

	case sage.INCLUDE:
		result := c.compile_include_node(node.(*sage.UnaryNode))
		if result.module_cat == MOD_ERR {
			return nil, result
		}

		return nil, NewResult(MOD_INCLUDE, "")

	case sage.COMPILE_TIME_EXECUTE:
		// NOTE: shouldn't we also be getting error logs from this as well?
		c.compile_compile_time_execute_node(node.(*sage.UnaryNode))
		return nil, NewResult(MOD_COMPILE_TIME, "")

	default:
		return nil, NewResult(MOD_ERR, fmt.Sprintf("COMPILATION ERROR: Cannot have %s in top level scope.", node.Get_true_nodetype()))
	}
}

func (c *SageCompiler) compile_include_node(node *sage.UnaryNode) *CompilationResult {
	include_file := node.Get_token().Lexeme

	// get rid of the '"' symbols wrapping the module name
	include_file = include_file[1 : len(include_file)-1]

	const SEARCH_PATH = "/home/alek/Desktop/projects/sage/modules/"
	full_filename := fmt.Sprintf("%s%s%s", SEARCH_PATH, include_file, ".sage")
	content, err := os.ReadFile(full_filename)
	if err != nil {
		return NewResult(MOD_ERR, fmt.Sprintf("LINKER ERROR: %s", err.Error()))
	}

	// NOTE: the way we will do includes is we read a sage file from the modules directory with
	//		 which will contain the functions or function definitions of all the functions in that module.
	//		 we then can generate the ir from that sage code and include it in the final .ll file.
	//		 further, 'include' statements inside of sage module files will not be allowed. later it *might* be suported, not sure yet.
	parser := sage.NewParser(SEARCH_PATH+include_file, content)
	parsetree := parser.Parse_program(false)

	c.compile_code(parsetree, true) // using true because we are compiling a module file

	return NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_binary_node(node *sage.BinaryNode) ([]IRInstructionProtocol, *CompilationResult) {
	// NOTE: compiles expression node which for now only deals with numeric values,
	//		 but could include terms that have func calls that return numeric values.

	var total_expression_ir []IRInstructionProtocol

	op_lexeme := node.Get_token().Lexeme
	expression_result_type, error := resolve_node_type(node)
	if error {
		return nil, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of expression: %s", node.String()))
	}

	// evalute both branches of nodes to extract operands and or generate any needed extra IR instructions
	leftside_ir, operand1, successful := c.visit_operand_branch(node.Left)
	if !successful {
		// in cases non-successful cases operand1 stores the error message string
		return nil, NewResult(MOD_ERR, operand1)
	}
	total_expression_ir = append(total_expression_ir, leftside_ir...)

	rightside_ir, operand2, successful := c.visit_operand_branch(node.Right)
	if !successful {
		// in cases non-successful cases operand2 stores the error message string
		return nil, NewResult(MOD_ERR, operand2)
	}
	total_expression_ir = append(total_expression_ir, rightside_ir...)

	// now generate the most current IR instruction
	op_name := op_lexeme_to_llvm_op_lexeme(expression_result_type, op_lexeme)

	// array literals are currently not allowed in expressions so setting length to -1
	llvm_type := sage_to_llvm_type(expression_result_type, -1)

	ir := &IRExpression{
		operation_name:  op_name,
		irtype:          llvm_type,
		operand1:        operand1,
		operand2:        operand2,
		result_register: c.current_scope.NewRegister(),
	}

	total_expression_ir = append(total_expression_ir, ir)

	return total_expression_ir, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) visit_operand_branch(branch_node sage.ParseNode) ([]IRInstructionProtocol, string, bool) {
	branchnode_type := branch_node.Get_true_nodetype()
	if branchnode_type == sage.NUMBER || branchnode_type == sage.FLOAT {
		operand := branch_node.Get_token().Lexeme
		return []IRInstructionProtocol{}, operand, true
	}

	branch_ir, result := c.compile_expression_operand(branch_node)
	if result.module_cat == MOD_ERR {
		return branch_ir, "", false
	}

	operand := branch_ir[len(branch_ir)-1].ResultRegister()
	return branch_ir, operand, true
}

func (c *SageCompiler) compile_expression_operand(node sage.ParseNode) ([]IRInstructionProtocol, *CompilationResult) {
	switch node.Get_true_nodetype() {
	case sage.BINARY:
		return c.compile_binary_node(node.(*sage.BinaryNode))
	case sage.FUNCCALL:
		call_ir, err_msg := c.compile_func_call_node(node.(*sage.UnaryNode))
		return call_ir, err_msg
	case sage.VAR_REF:
		ref_ir, err_msg := c.compile_var_ref_node(node.(*sage.UnaryNode))
		return []IRInstructionProtocol{ref_ir}, err_msg
	}

	return []IRInstructionProtocol{}, NewResult(MOD_ERR, "COMPILATION ERROR: Invalid operand found in expression.")
}

// func (c *SageCompiler) compile_unary_node(node *sage.UnaryNode) ([]IRInstructionProtocol, string) {}

func (c *SageCompiler) compile_string_node(node *sage.UnaryNode) (*IRAtom, *CompilationResult) {
	// NOTE: node is representing a string literal

	node_literal_value := node.Get_token().Lexeme
	literal_internal_name := CreateInternalName()
	ir_type := fmt.Sprintf("[ %d x i8 ]", len(node_literal_value))

	c.current_scope.scope.AddSymbol(literal_internal_name, VARIABLE, &AtomicValue{ARRAY_CHAR, node_literal_value, nil}, ARRAY_CHAR)
	symbol, _ := c.current_scope.scope.LookupSymbol(literal_internal_name)

	last_param := false
	if c.current_scope.ast_type == FUNCTION {
		// when generating the IR it is important to know if a param is the last because we need to know if a ',' follows it in the IR or not
		last_param = c.current_scope.parameter_index == c.current_scope.parameter_amount-1
	}

	ir := &IRAtom{
		name:             literal_internal_name,
		irtype:           ir_type,
		value:            node_literal_value,
		instruction_type: STRLITERAL,
		is_last_param:    last_param,
		result_register:  symbol.NewRegister(),
	}

	return ir, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_keyword_node(node *sage.UnaryNode) ([]IRInstructionProtocol, *CompilationResult) {
	// NOTE: this function assumes the keyword is "return", the future more keyword statements could exist and checks would be needed for those
	var return_instructions []IRInstructionProtocol

	if node.Get_child_node() != nil {
		// if a child node is present then that means we are returning a value, so we need to compile the IR required for that as well
		return_node := node.Get_child_node()
		switch return_node.Get_true_nodetype() {
		case sage.STRING:
			literal_ir, result := c.compile_string_node(return_node.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return []IRInstructionProtocol{literal_ir}, result
			}

			return_instructions = append(return_instructions, literal_ir)

		case sage.VAR_REF:
			ref_ir, result := c.compile_var_ref_node(return_node.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return []IRInstructionProtocol{ref_ir}, result
			}

			return_instructions = append(return_instructions, ref_ir)

		case sage.NUMBER: // if its a number then it will just compile the same as float would
		case sage.FLOAT:
			raw_value := node.Get_child_node().Get_token().Lexeme
			node_type, failed := resolve_node_type(node)
			if failed {
				return return_instructions, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of expression: %s.", node.String()))
			}

			scope_return_type := c.current_scope.sage_datatype
			if node_type != scope_return_type {
				return return_instructions, NewResult(MOD_ERR, "Mismatching return types found.")
			}

			// using current scope's return type because this is a return statement so we will assume the return type the parent function is returning
			return_ir := &IRAtom{
				irtype:           c.current_scope.sage_datatype_to_llvm(),
				value:            raw_value,
				instruction_type: RET_STMT,
			}
			return_instructions = append(return_instructions, return_ir)

			return return_instructions, NewResult(MOD_NONE, "")

		case sage.FUNCCALL:
			call_ir, result := c.compile_func_call_node(return_node.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return []IRInstructionProtocol{}, result
			}

			return_instructions = append(return_instructions, call_ir...)

		case sage.BINARY: // expression
			expression_ir, result := c.compile_binary_node(return_node.(*sage.BinaryNode))
			if result.module_cat == MOD_ERR {
				return expression_ir, result
			}

			return_instructions = append(return_instructions, expression_ir...)
		}

		result_reg := return_instructions[len(return_instructions)-1].ResultRegister()
		return_ir := &IRAtom{
			irtype:           c.current_scope.sage_datatype_to_llvm(),
			value:            result_reg,
			instruction_type: RET_STMT,
		}
		return_instructions = append(return_instructions, return_ir)

		return return_instructions, NewResult(MOD_NONE, "")
	}

	// irtype and value arent needed here because if the return has nothing attached then it will always be returning void
	return_ir := &IRAtom{
		instruction_type: RET_STMT,
	}

	return_instructions = append(return_instructions, return_ir)

	return return_instructions, NewResult(MOD_NONE, "")
}

// TODO: simplify this function as well
func (c *SageCompiler) compile_block_node(node *sage.BlockNode, block_label string) ([]IRBlock, *CompilationResult) {
	var blocks []IRBlock
	var block_contents []IRInstructionProtocol
	var main_block IRBlock

	for _, childnode := range node.Children {
		switch childnode.Get_true_nodetype() {
		case sage.KEYWORD:
			keyword_ir, result := c.compile_keyword_node(childnode.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, keyword_ir...)

		case sage.FUNCDEF:
			def_ir, result := c.compile_func_def_node(childnode.(*sage.BinaryNode))
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, def_ir)

		case sage.FUNCCALL:
			call_ir, result := c.compile_func_call_node(childnode.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, call_ir...)

		case sage.IF:
			conditional_ir, result := c.compile_if_node(childnode)
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			blocks = append(blocks, conditional_ir.all_blocks...)

		case sage.WHILE:
			while_ir, result := c.compile_while_node(childnode)
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			blocks = append(blocks, while_ir.all_blocks...)

		case sage.FOR:
			for_ir, result := c.compile_for_node(childnode)
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			blocks = append(blocks, for_ir.all_blocks...)

		case sage.ASSIGN:
			assign_ir, result := c.compile_assign_node(childnode)
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, assign_ir...)

		case sage.STRUCT:
			struct_ir, result := c.compile_struct_node(childnode)
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, struct_ir)

		case sage.VAR_DEC:
			dec_ir, result := c.compile_var_dec_node(childnode.(*sage.BinaryNode))
			if result.module_cat == MOD_ERR {
				return blocks, result
			}

			block_contents = append(block_contents, dec_ir)

		case sage.COMPILE_TIME_EXECUTE:
			c.compile_compile_time_execute_node(childnode.(*sage.UnaryNode))

		default:
			return blocks, NewResult(MOD_ERR, "COMPILATION ERROR: Found statement that does not make sense within context of a block.")
		}

		main_block = IRBlock{
			Name: block_label,
			Body: block_contents,
		}
	}

	blocks = append(blocks, main_block)
	return blocks, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_param_list_node(node *sage.BlockNode, found_vararg *bool) ([]IRAtom, *CompilationResult) {
	var params []IRAtom
	var binary *sage.BinaryNode
	var param_name string
	var sage_type SAGE_DATATYPE
	var failed bool
	var output_code TableOutput
	var param_symbol *Symbol
	for index, childnode := range node.Children {
		c.current_scope.parameter_index = index

		if childnode.Get_true_nodetype() == sage.VARARG {
			*found_vararg = true
		}

		binary = childnode.(*sage.BinaryNode)
		param_name = binary.Left.Get_token().Lexeme

		sage_type, failed = resolve_node_type(binary.Right)
		if failed {
			return params, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of parameter: %s.", binary.Right.String()))
		}

		output_code = c.current_scope.scope.AddSymbol(param_name, PARAMETER, nil, sage_type)
		if output_code == NAME_COLLISION {
			return params, NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of parameter: %s.n", param_name))
		}

		param_symbol, _ = c.current_scope.scope.LookupSymbol(param_name)

		// all children inside a funcdef param list will be a BinaryNode
		param := IRAtom{
			name:             param_name,
			irtype:           param_symbol.sage_datatype_to_llvm(),
			instruction_type: PARAM,
		}
		params = append(params, param)
	}

	return params, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_func_dec_node(node *sage.BinaryNode) (IRFunc, *CompilationResult) {
	// NOTE: could probably combine this somehow with compile_func_def_node, they re-use a lot of the same code
	function_signature := node.Right.(*sage.BinaryNode)
	parameters := function_signature.Left.(*sage.BlockNode)
	function_name := node.Left.Get_token().Lexeme

	function_return_type, failed := resolve_node_type(function_signature.Right)
	if failed {
		return IRFunc{}, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of function return type: %s.", function_signature.Right.String()))
	}

	output_code := c.current_scope.scope.AddSymbol(function_name, FUNCTION, nil, function_return_type)
	if output_code == NAME_COLLISION {
		return IRFunc{}, NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of function: %s.", function_name))
	}

	func_symbol, _ := c.current_scope.scope.LookupSymbol(function_name)

	// initialize the function symbol scope
	func_symbol.scope = &SymbolTable{}

	// create function visitor
	func_symbol.parameter_index = 0
	func_symbol.parameter_amount = len(parameters.Children)

	// Enter nested function scope
	previous_scope := c.current_scope
	c.current_scope = func_symbol

	// declare on the heap to share across the function
	found_vararg := new(bool)
	*found_vararg = false

	function_params, result := c.compile_param_list_node(parameters, found_vararg)
	if result.module_cat == MOD_ERR {
		return IRFunc{}, result
	}

	// Leave nested function scope
	c.current_scope = previous_scope

	// generate ir
	ir := IRFunc{
		name:         function_name,
		return_type:  func_symbol.sage_datatype_to_llvm(),
		parameters:   function_params,
		calling_conv: "NOCONV",
		attribute:    "NOATTRIBUTE",
		body:         nil,
		is_vararg:    *found_vararg,
	}

	return ir, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_func_def_node(node *sage.BinaryNode) (IRFunc, *CompilationResult) {
	// all function defs are represented as a binary node in the parse tree, with the Left being the function name and the Right being a trinary node that represents the function signature
	function_signature := node.Right.(*sage.TrinaryNode)
	parameters := function_signature.Left.(*sage.BlockNode)
	function_name := node.Left.Get_token().Lexeme

	function_return_type, failed := resolve_node_type(function_signature.Middle)
	if failed {
		return IRFunc{}, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of function return type: %s.", function_signature.Middle.String()))
	}

	output_code := c.current_scope.scope.AddSymbol(function_name, FUNCTION, nil, function_return_type)
	if output_code == NAME_COLLISION {
		return IRFunc{}, NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of function: %s.", function_name))
	}

	func_symbol, _ := c.current_scope.scope.LookupSymbol(function_name)

	// initialize the function symbol scope
	func_symbol.scope = &SymbolTable{}

	// create function visitor
	func_symbol.parameter_index = 0
	func_symbol.parameter_amount = len(parameters.Children)

	// Enter nested function scope
	previous_scope := c.current_scope
	prev_scope_content := previous_scope.scope
	c.current_scope = func_symbol
	c.current_scope.scope = prev_scope_content

	// declare on the heap to share across the function
	found_vararg := new(bool)
	*found_vararg = false

	function_params, result := c.compile_param_list_node(parameters, found_vararg)
	if result.module_cat == MOD_ERR {
		return IRFunc{}, result
	}

	// the body could be omitted if it is simply a function declaration and not a definition
	var function_body []IRBlock
	if function_signature.Right != nil {
		body := function_signature.Right.(*sage.BlockNode)
		function_body, result = c.compile_block_node(body, "entry")
		if result.module_cat == MOD_ERR {
			return IRFunc{}, result
		}
	}

	// Leave nested function scope
	c.current_scope = previous_scope

	// generate ir
	ir := IRFunc{
		name:         function_name,
		return_type:  func_symbol.sage_datatype_to_llvm(),
		parameters:   function_params,
		calling_conv: "NOCONV",
		attribute:    "NOATTRIBUTE",
		body:         function_body,
		is_vararg:    *found_vararg,
	}

	return ir, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_func_call_node(node *sage.UnaryNode) ([]IRInstructionProtocol, *CompilationResult) {
	var retval []IRInstructionProtocol
	function_name := node.Get_token().Lexeme

	// get parameter block node to update visitor
	parameters_raw := node.Get_child_node() // should return BlockNode
	parameters := parameters_raw.(*sage.BlockNode)

	// lookup function in symbol table
	symbol, output_code := c.current_scope.scope.LookupSymbol(function_name)
	if output_code == NAME_UNDEFINED {
		return retval, NewResult(MOD_ERR, fmt.Sprintf("Invalid reference of undefined function: %s.", function_name))
	}

	// get []IRAtom struct list from parameters
	var ir_parameters []IRInstructionProtocol
	for _, parameter := range parameters.Children {
		// there is more freedom on what values can be passed into a function, theoretically any kind of primary or expression could be passed as a parameter
		instructions, result := c.compile_primary_node(parameter)
		if result.module_cat != MOD_NONE {
			return retval, result
		}

		retval = append(retval, instructions...)

		resultant_inst := instructions[len(instructions)-1]
		register := resultant_inst.ResultRegister()
		irtype, _ := resultant_inst.TypeInfo()

		param_ir := IRAtom{
			name:             register,
			irtype:           irtype,
			instruction_type: PARAM,
		}

		ir_parameters = append(ir_parameters, param_ir)
	}

	// allocate function call result to a register
	result_reg := symbol.NewRegister()

	// get the function's return type
	// TODO: make it so that we are generating the full function type
	return_type := symbol.sage_datatype_to_llvm()

	ir := IRFuncCall{
		tail:            false,
		calling_conv:    "NOCONV",
		return_type:     return_type,
		name:            function_name,
		result_register: result_reg,
		parameters:      ir_parameters,
	}
	retval = append(retval, ir)

	return retval, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_struct_node(node sage.ParseNode) (IRStruct, *CompilationResult) {
	return IRStruct{}, NewResult(MOD_ERR, "")
}

func (c *SageCompiler) compile_if_node(node sage.ParseNode) (IRConditional, *CompilationResult) {
	return IRConditional{}, NewResult(MOD_ERR, "")
}

func (c *SageCompiler) compile_while_node(node sage.ParseNode) (IRWhile, *CompilationResult) {
	return IRWhile{}, NewResult(MOD_ERR, "")

}

// TODO: modularize and make this function readable and smaller
func (c *SageCompiler) compile_assign_node(node sage.ParseNode) ([]IRInstructionProtocol, *CompilationResult) {
	var retval []IRInstructionProtocol
	if node.Get_nodetype() == sage.BINARY {
		// read left hand side
		binary := node.(*sage.BinaryNode)
		store_ident := binary.Left.Get_token().Lexeme

		// read right hand side
		rhs_ir, result := c.compile_primary_node(binary.Right)
		if result.module_cat != MOD_NONE {
			return retval, result
		}
		retval = append(retval, rhs_ir...)

		// find the type of the expression
		statement_type, error := resolve_node_type(binary.Right)
		if error {
			// return retval, fmt.Sprintf("COMPILATION ERROR: Could not resolve the type of node %s\n", binary.Right.String())
			return retval, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of expression: %s.", binary.Right.String()))
		}

		// store the value of the right hand side
		symbol, output_code := c.current_scope.scope.LookupSymbol(store_ident)
		if output_code == NAME_UNDEFINED {
			return retval, NewResult(MOD_ERR, fmt.Sprintf("Invalid reference to undefined variable: %s.", store_ident))
		}

		// check that we aren't assigning a mismatching type
		if symbol.sage_datatype != statement_type {
			return retval, NewResult(MOD_ERR, fmt.Sprintf("Invalid attempt to assign %s type to %s type.", statement_type, symbol.sage_datatype))
		}

		store_instruction := IRAtom{
			name:             store_ident,
			value:            rhs_ir[len(rhs_ir)-1].ResultRegister(),
			irtype:           symbol.sage_datatype_to_llvm(),
			instruction_type: STORE,
		}

		retval = append(retval, store_instruction)
		return retval, NewResult(MOD_NONE, "")

	} else if node.Get_nodetype() == sage.TRINARY {
		// read left hand side
		trinary := node.(*sage.TrinaryNode)
		assign_name := trinary.Left.Get_token().Lexeme

		// create IR for right hand side
		rhs_inst, result := c.compile_primary_node(trinary.Right)
		if result.module_cat != MOD_NONE {
			return retval, result
		}

		// find the type of the right hand side
		rhs_type, error := resolve_node_type(trinary.Right)
		if error {
			return retval, NewResult(MOD_ERR, "COMPILATION ERROR: Was unable to resolve type of rhs expression in assign statement.")
		}
		retval = append(retval, rhs_inst...)

		assignment_type, failed := resolve_node_type(trinary.Middle)
		if failed {
			return retval, NewResult(MOD_ERR, fmt.Sprintf("Could not resovle type of expression: %s.", trinary.Middle.String()))
		}

		if assignment_type != rhs_type {
			// check that what we are assigning matches the type of the new variable
			// return retval,
			return retval, NewResult(MOD_ERR, fmt.Sprintf("COMPILATION ERROR: Type mismatch found, cannot assign %s type to %s type.", assignment_type, rhs_type))
		}

		// update the scope with the existence of this new variable
		output_code := c.current_scope.scope.AddSymbol(assign_name, VARIABLE, nil, assignment_type)
		if output_code == NAME_COLLISION {
			return retval, NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of variable: %s.", assign_name))
		}

		symbol, _ := c.current_scope.scope.LookupSymbol(assign_name)
		llvm_type := symbol.sage_datatype_to_llvm()

		// then create the ir for the lhs (alloca and store)
		stack_allocate := IRAtom{
			name:             assign_name,
			irtype:           llvm_type,
			instruction_type: INIT,
		}
		retval = append(retval, stack_allocate)

		store_instruction := IRAtom{
			name:             assign_name,
			value:            rhs_inst[len(rhs_inst)-1].ResultRegister(),
			irtype:           llvm_type,
			instruction_type: STORE,
		}
		retval = append(retval, store_instruction)

		// package all the ir blocks in order into a single list ir block
		return retval, NewResult(MOD_NONE, "")
	}

	return retval, NewResult(MOD_ERR, fmt.Sprintf("COMPILATION ERROR: Cannot compile assign node: %s.", node))
}

func (c *SageCompiler) compile_primary_node(node sage.ParseNode) ([]IRInstructionProtocol, *CompilationResult) {
	truetype := node.Get_true_nodetype()
	retval := []IRInstructionProtocol{}
	switch truetype {
	case sage.STRING:
		ir, cat := c.compile_string_node(node.(*sage.UnaryNode))
		retval = append(retval, ir)
		return retval, cat

	case sage.FUNCCALL:
		ir, cat := c.compile_func_call_node(node.(*sage.UnaryNode))
		retval = append(retval, ir...)
		return retval, cat

	case sage.VAR_REF:
		ir, cat := c.compile_var_ref_node(node.(*sage.UnaryNode))
		retval = append(retval, ir)
		return retval, cat

	case sage.BINARY:
		// it is an expression
		ir, cat := c.compile_binary_node(node.(*sage.BinaryNode))
		retval = append(retval, ir...)
		return retval, cat

	default:
		return retval, NewResult(MOD_ERR, fmt.Sprintf("COMPILATION ERROR: Could not compile primary node: %s.", node))
	}
}

func (c *SageCompiler) compile_for_node(node sage.ParseNode) (IRFor, *CompilationResult) {
	return IRFor{}, NewResult(MOD_ERR, "")
}

// func (c *SageCompiler) compile_range_node(node sage.ParseNode) ([]IRInstructionProtocol, string) {}

func (c *SageCompiler) compile_var_dec_node(node *sage.BinaryNode) (*IRAtom, *CompilationResult) {
	sage_type, err := resolve_node_type(node)
	if err {
		return nil, NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of for loop variable: %s.", node.String()))
	}

	var_name := node.Left.Get_token().Lexeme

	// update symbol table and register tracker with new variable
	c.current_scope.scope.AddSymbol(var_name, VARIABLE, nil, sage_type)
	symbol, _ := c.current_scope.scope.LookupSymbol(var_name)
	assignment_type := symbol.sage_datatype_to_llvm()

	stack_allocate := IRAtom{
		name:             var_name,
		irtype:           assignment_type,
		instruction_type: INIT,
	}

	return &stack_allocate, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_var_ref_node(node *sage.UnaryNode) (*IRAtom, *CompilationResult) {
	var_name := node.Get_token().Lexeme

	symbol, output := c.current_scope.scope.LookupSymbol(var_name)
	if output == NAME_UNDEFINED {
		return nil, NewResult(MOD_ERR, fmt.Sprintf("Invalid reference to variable: %s.", var_name))
	}

	reg := symbol.NewRegister()

	ir_type := symbol.sage_datatype_to_llvm()

	load_inst := IRAtom{
		name:             var_name,
		irtype:           ir_type,
		instruction_type: REF,
		result_register:  reg,
	}
	return &load_inst, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_compile_time_execute_node(node *sage.UnaryNode) {
	block_node := node.Get_child_node().(*sage.BlockNode)
	for _, childnode := range block_node.Children {
		c.interpreter.interpret(childnode)
	}
}

func resolve_node_type(node sage.ParseNode) (datatype SAGE_DATATYPE, error bool) {
	switch node.Get_true_nodetype() {
	case sage.NUMBER:
		return I32, false

	case sage.FLOAT:
		return F32, false

	case sage.BINARY:
		binary := node.(*sage.BinaryNode)
		op1_type, error := resolve_node_type(binary.Left)
		if error {
			return op1_type, true
		}

		op2_type, error := resolve_node_type(binary.Right)
		if error {
			return op2_type, true
		}

		if op1_type == F32 || op2_type == F32 {
			return F32, false
		}

		return I32, false

	case sage.UNARY:
		unary := node.(*sage.UnaryNode)
		childnode := unary.Get_child_node()
		tag := unary.Tag

		if childnode == nil {
			return STRUCT_TYPE, false
		}

		inner_type, err := resolve_node_type(childnode)
		if err {
			return VOID, true
		}

		pointer_map := map[SAGE_DATATYPE]SAGE_DATATYPE{
			INT:  POINTER_I64,
			BOOL: POINTER_BOOL,
			I16:  POINTER_I16,
			I32:  POINTER_I32,
			I64:  POINTER_I64,
			F32:  POINTER_F32,
			F64:  POINTER_F64,
			CHAR: POINTER_CHAR,
		}

		array_map := map[SAGE_DATATYPE]SAGE_DATATYPE{
			INT:  ARRAY_I64,
			BOOL: ARRAY_BOOL,
			I16:  ARRAY_I16,
			I32:  ARRAY_I32,
			I64:  ARRAY_I64,
			F32:  ARRAY_F32,
			F64:  ARRAY_F64,
			CHAR: ARRAY_CHAR,
		}

		if tag == "pointer_to_" {
			return pointer_map[inner_type], false
		} else {
			// otherwise assume its an array type, function type will be implemented later
			return array_map[inner_type], false
		}

	case sage.TYPE, sage.VARARG: // NOTE: for now vararg will just evaluate the same as a named type
		datatype, exists := str_literal_to_sagetype(node.Get_token().Lexeme)
		if !exists {
			return VOID, true
		}

		return datatype, false

	default:
		// return fmt.Sprintf("COMPILATION ERROR: CANNOT RESOLVE TYPE %s\n", node.Get_token().Lexeme), true
		return VOID, true
	}
}

func op_lexeme_to_llvm_op_lexeme(result_type SAGE_DATATYPE, op_string string) string {
	var div_mnemonic string = "sdiv"
	var mul_mnemonic string = "mul"
	if result_type == F32 || result_type == F64 {
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

func sage_to_llvm_type(sagetype SAGE_DATATYPE, length int) string {
	typemap := map[SAGE_DATATYPE]string{
		INT:  "i32",
		BOOL: "i1",
		I16:  "i16",
		I32:  "i32",
		I64:  "i64",
		F32:  "float32",
		F64:  "float64",
		CHAR: "i8",
		VOID: "void",

		POINTER_I16:  "i16*",
		POINTER_I32:  "i32*",
		POINTER_I64:  "i64*",
		POINTER_F32:  "float32*",
		POINTER_F64:  "float64*",
		POINTER_CHAR: "i8*",
		POINTER_BOOL: "i1*",

		ARRAY_I16:  fmt.Sprintf("[ %d x i16 ]", length),
		ARRAY_I32:  fmt.Sprintf("[ %d x i32 ]", length),
		ARRAY_I64:  fmt.Sprintf("[ %d x i64 ]", length),
		ARRAY_F32:  fmt.Sprintf("[ %d x float32 ]", length),
		ARRAY_F64:  fmt.Sprintf("[ %d x float64 ]", length),
		ARRAY_CHAR: fmt.Sprintf("[ %d x i8 ]", length),
		ARRAY_BOOL: fmt.Sprintf("[ %d x i1 ]", length),
	}

	return typemap[sagetype]
}

func str_literal_to_sagetype(str string) (retval SAGE_DATATYPE, exists bool) {
	typemap := map[string]SAGE_DATATYPE{
		"int":  INT,
		"bool": BOOL,
		"i16":  I16,
		"i32":  I32,
		"i64":  I64,
		"f32":  F32,
		"f64":  F64,
		"char": CHAR,
		"void": VOID,

		// NOTE: might not need these because of how pointers are parsed
		"int*":  POINTER_I64,
		"bool*": POINTER_BOOL,
		"i16*":  POINTER_I16,
		"i32*":  POINTER_I32,
		"i64*":  POINTER_I64,
		"f32*":  POINTER_F32,
		"f64*":  POINTER_F64,
		"char*": POINTER_CHAR,
	}

	datatype, exists := typemap[str]
	return datatype, exists
}
