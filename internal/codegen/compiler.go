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
	ir_buffer     []IRInstructionProtocol
	// TODO: create Logger object
}

func (c *SageCompiler) push(inst IRInstructionProtocol) {
	c.ir_buffer = append([]IRInstructionProtocol{inst}, c.ir_buffer...)
}

func (c *SageCompiler) pop() IRInstructionProtocol {
	retval := c.ir_buffer[0]
	c.ir_buffer = c.ir_buffer[1:]
	return retval
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

	compiler := SageCompiler{interpreter, code_module, global_scope, []IRInstructionProtocol{}}
	compiler.compile_code(parsetree, false) // using false because this function is meant to only compile sage project source files

	// TODO: create the .ll file and emit the IR from the ir_module
	//		 might want to create new function for this.
	compiler.emit_ir()
}

func (c *SageCompiler) emit_ir() {
	build_settings, _ := c.current_scope.scope.LookupSymbol("build_settings")
	raw, exists := build_settings.value.struct_map["executable_name"]
	var executable_filename string
	if !exists || raw == nil {
		executable_filename = "a.out"
	} else {
		// if the name does exist then retrieve it and remove the wrapping '"' symbols around the name
		executable_filename = raw.(string)
		executable_filename = remove_wrapping_quotes(executable_filename)
		// executable_filename = executable_filename[1 : len(executable_filename)-1]
	}

	platform_selection, _ := build_settings.value.struct_map["platform"].(string)
	architecture_selection, _ := build_settings.value.struct_map["architecture"].(string)
	bitsize_selection, _ := build_settings.value.struct_map["bitsize"].(string)

	datalayout, triple, success := set_datalayouts(platform_selection, architecture_selection, bitsize_selection)
	if !success {
		fmt.Println("Unable to find valid platform, architecture or bitsize selection.")
		return
	}
	c.ir_module.target_datalayout = datalayout
	c.ir_module.target_triple = triple

	llvm_filename := fmt.Sprintf("%s.ll", executable_filename)
	object_filename := fmt.Sprintf("%s.o", executable_filename)

	// create the IR file with the IR
	llvm_ir := c.ir_module.ToLLVM()
	err := os.WriteFile(llvm_filename, []byte(llvm_ir), 0755)
	if err != nil {
		fmt.Println("Could not write to LLVM file.")
		return
	}

	const TESTING_IR_GENERATION = false
	if TESTING_IR_GENERATION {
		return
	}

	// [1] llc -relocation-model=pic -filetype=obj add.ll -o objfile
	// [2] gcc objfile -o exec

	_, err = exec.Command("llc", "-relocation-model=pic", "-filetype=obj", llvm_filename, "-o", object_filename).Output()
	if err != nil {
		fmt.Println("Could not create object file.")
		fmt.Println(err.Error())
		return
	}

	_, err = exec.Command("gcc", object_filename, "-o", executable_filename).Output()
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
}

func (c *SageCompiler) compile_code(parsetree sage.ParseNode, is_module_file bool) {
	program_root_node := parsetree.(*sage.BlockNode)
	for count, node := range program_root_node.Children {
		fmt.Printf("[%d] compiling node: %s...\n", count, node.String())
		result := c.compile_general_node(node, is_module_file)

		// NOTE: this method of error reporting is temporary, will be replaced with logger data structure
		if result.module_cat == MOD_ERR {
			fmt.Println(result.error)
		}
	}
}

// TODO: simplify this function. it is very repetative, could make the cases into a single function thats called in each case with different parameters
func (c *SageCompiler) compile_general_node(node sage.ParseNode, is_module_file bool) *CompilationResult {
	switch node.Get_true_nodetype() {
	case sage.FUNCDEF:
		result := c.compile_func_def_node(node.(*sage.BinaryNode))
		if result.module_cat == MOD_ERR {
			return result
		}

		return NewResult(MOD_FDEF, "")
	case sage.FUNCDEC:
		result := c.compile_func_dec_node(node.(*sage.BinaryNode))
		if result.module_cat == MOD_ERR {
			return result
		}

		return NewResult(MOD_FDEC, "")

	case sage.STRUCT:
		result := c.compile_struct_node(node)
		if result.module_cat == MOD_ERR {
			return result
		}

		return NewResult(MOD_STRUCT, "")

	case sage.INCLUDE:
		if is_module_file {
			return NewResult(MOD_ERR, "Include statements are not allowed inside included module files.")
		}

		result := c.compile_include_node(node.(*sage.UnaryNode))
		if result.module_cat == MOD_ERR {
			return result
		}

		return NewResult(MOD_INCLUDE, "")

	case sage.COMPILE_TIME_EXECUTE:
		// NOTE: shouldn't we also be getting error logs from this as well?
		c.compile_compile_time_execute_node(node.(*sage.UnaryNode))
		return NewResult(MOD_COMPILE_TIME, "")

	default:
		return NewResult(MOD_ERR, fmt.Sprintf("COMPILATION ERROR: Cannot have %s in top level scope.", node.Get_true_nodetype()))
	}
}

func (c *SageCompiler) compile_include_node(node *sage.UnaryNode) *CompilationResult {
	include_file := node.Get_token().Lexeme

	// get rid of the '"' symbols wrapping the module name
	// include_file = include_file[1 : len(include_file)-1]
	include_file = remove_wrapping_quotes(include_file)

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

func (c *SageCompiler) compile_string_node(node *sage.UnaryNode) *CompilationResult {
	// NOTE: node is representing a string literal

	node_literal_value := sage_literal_to_llvm_literal(node.Get_token().Lexeme)
	literal_internal_name := CreateInternalGlobalName()
	ir_type := fmt.Sprintf("[ %d x i8 ]", len(remove_wrapping_quotes(node.Get_token().Lexeme)))

	c.current_scope.scope.AddSymbol(literal_internal_name, VARIABLE, &AtomicValue{ARRAY_CHAR, node_literal_value, nil}, ARRAY_CHAR)

	last_param := false
	if c.current_scope.ast_type == FUNCTION {
		// when generating the IR it is important to know if a param is the last because we need to know if a ',' follows it in the IR or not
		last_param = c.current_scope.parameter_index == c.current_scope.parameter_amount-1
	}

	ir := &IRAtom{
		name:             literal_internal_name,
		irtype:           ir_type,
		value:            node_literal_value,
		instruction_type: IT_STRLITERAL,
		is_last_param:    last_param,
		result_register:  literal_internal_name,
	}

	c.ir_module.Globals = append(c.ir_module.Globals, ir)
	c.push(ir)

	return NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_keyword_node(node *sage.UnaryNode) ([]IRInstructionProtocol, *CompilationResult) {
	// NOTE: this function assumes the keyword is "return", the future more keyword statements could exist and checks would be needed for those
	var return_instructions []IRInstructionProtocol

	if node.Get_child_node() != nil {
		// if a child node is present then that means we are returning a value, so we need to compile the IR required for that as well
		return_node := node.Get_child_node()
		switch return_node.Get_true_nodetype() {
		case sage.STRING:
			result := c.compile_string_node(return_node.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return []IRInstructionProtocol{}, result
			}

			literal_ir := c.pop()
			return_instructions = append(return_instructions, literal_ir)

		case sage.VAR_REF:
			ref_ir, result := c.compile_var_ref_node(return_node.(*sage.UnaryNode))
			if result.module_cat == MOD_ERR {
				return []IRInstructionProtocol{ref_ir}, result
			}

			return_instructions = append(return_instructions, ref_ir)

		case sage.FLOAT, sage.NUMBER: // if its a number then it will just compile the same as float would
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
				instruction_type: IT_RET_STMT,
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
			instruction_type: IT_RET_STMT,
		}
		return_instructions = append(return_instructions, return_ir)

		return return_instructions, NewResult(MOD_NONE, "")
	}

	// irtype and value arent needed here because if the return has nothing attached then it will always be returning void
	return_ir := &IRAtom{
		instruction_type: IT_RET_STMT,
	}

	return_instructions = append(return_instructions, return_ir)

	return return_instructions, NewResult(MOD_NONE, "")
}

// TODO: simplify this function as well
func (c *SageCompiler) compile_block_node(node *sage.BlockNode, block_label string) ([]IRBlock, *CompilationResult) {
	var blocks, result_blocks []IRBlock
	var block_contents, result_ir []IRInstructionProtocol
	var main_block IRBlock
	var result *CompilationResult
	var adding_blocks bool

	for _, childnode := range node.Children {
		switch childnode.Get_true_nodetype() {
		case sage.KEYWORD:
			result_ir, result = c.compile_keyword_node(childnode.(*sage.UnaryNode))
			adding_blocks = false

		case sage.FUNCCALL:
			result_ir, result = c.compile_func_call_node(childnode.(*sage.UnaryNode))
			adding_blocks = false

		case sage.IF:
			var conditional_ir IRConditional
			conditional_ir, result = c.compile_if_node(childnode)
			result_blocks = conditional_ir.all_blocks
			adding_blocks = true

		case sage.WHILE:
			var while_ir IRWhile
			while_ir, result = c.compile_while_node(childnode)
			result_blocks = while_ir.all_blocks
			adding_blocks = true

		case sage.FOR:
			var for_ir IRFor
			for_ir, result = c.compile_for_node(childnode)
			result_blocks = for_ir.all_blocks
			adding_blocks = true

		case sage.ASSIGN:
			result_ir, result = c.compile_assign_node(childnode)
			adding_blocks = false

		case sage.VAR_DEC:
			var ir IRInstructionProtocol
			ir, result = c.compile_var_dec_node(childnode.(*sage.BinaryNode))
			result_ir = []IRInstructionProtocol{ir}
			adding_blocks = false

		case sage.COMPILE_TIME_EXECUTE:
			c.compile_compile_time_execute_node(childnode.(*sage.UnaryNode))
			adding_blocks = false

		default:
			return blocks, NewResult(MOD_ERR, "COMPILATION ERROR: Found statement that does not make sense within context of a block.")
		}

		if result.module_cat == MOD_ERR {
			return blocks, result
		}

		if adding_blocks {
			blocks = append(blocks, result_blocks...)
		} else {
			block_contents = append(block_contents, result_ir...)
		}
	}

	main_block = IRBlock{
		Name: block_label,
		Body: block_contents,
	}

	blocks = append(blocks, main_block)
	return blocks, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_param_list_node(node *sage.BlockNode, found_vararg *bool, instruction_type VarInstructionType) ([]IRAtom, *CompilationResult) {
	var params []IRAtom
	var binary *sage.BinaryNode
	var param_name string
	var sage_type SAGE_DATATYPE
	var failed bool
	var output_code TableOutput
	var param_symbol *Symbol
	var builder strings.Builder
	builder.WriteString("(")

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
			return params, NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of parameter: %s.", param_name))
		}

		atom := &AtomicValue{sage_type, param_name, nil}
		c.current_scope.value.result_value = append(c.current_scope.value.result_value.([]*AtomicValue), atom)

		param_symbol, _ = c.current_scope.scope.LookupSymbol(param_name)
		irtype := param_symbol.sage_datatype_to_llvm()
		builder.WriteString(irtype)
		if index != len(node.Children)-1 {
			builder.WriteString(", ")
		}

		// all children inside a funcdef param list will be a BinaryNode
		param := IRAtom{
			name:             "%" + param_name,
			irtype:           param_symbol.sage_datatype_to_llvm(),
			instruction_type: instruction_type,
		}
		params = append(params, param)
	}
	builder.WriteString(")")
	c.current_scope.parameter_signature = builder.String()

	return params, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_func_dec_node(node *sage.BinaryNode) *CompilationResult {
	// NOTE: could probably combine this somehow with compile_func_def_node, they re-use a lot of the same code
	function_signature := node.Right.(*sage.BinaryNode)
	parameters := function_signature.Left.(*sage.BlockNode)
	function_name := node.Left.Get_token().Lexeme

	function_return_type, failed := resolve_node_type(function_signature.Right)
	if failed {
		return NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of function return type: %s.", function_signature.Right.String()))
	}

	output_code := c.current_scope.scope.AddSymbol(function_name, FUNCTION, nil, function_return_type)
	if output_code == NAME_COLLISION {
		return NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of function: %s.", function_name))
	}

	func_symbol, _ := c.current_scope.scope.LookupSymbol(function_name)

	// initialize the function symbol scope
	func_symbol.scope = &SymbolTable{}
	func_symbol.value = &AtomicValue{-1, []*AtomicValue{}, nil}

	// create function visitor
	func_symbol.parameter_index = 0
	func_symbol.parameter_amount = len(parameters.Children)

	// Enter nested function scope
	previous_scope := c.current_scope
	c.current_scope = func_symbol

	// declare on the heap to share across the function
	found_vararg := new(bool)
	*found_vararg = false

	function_params, result := c.compile_param_list_node(parameters, found_vararg, IT_PARAM_TYPE)
	if result.module_cat == MOD_ERR {
		return result
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

	c.ir_module.FuncDecs = append(c.ir_module.FuncDecs, ir)

	return NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_func_def_node(node *sage.BinaryNode) *CompilationResult {
	// all function defs are represented as a binary node in the parse tree, with the Left being the function name and the Right being a trinary node that represents the function signature
	function_signature := node.Right.(*sage.TrinaryNode)
	parameters := function_signature.Left.(*sage.BlockNode)
	function_name := node.Left.Get_token().Lexeme

	function_return_type, failed := resolve_node_type(function_signature.Middle)
	if failed {
		return NewResult(MOD_ERR, fmt.Sprintf("Could not resolve type of function return type: %s.", function_signature.Middle.String()))
	}

	output_code := c.current_scope.scope.AddSymbol(function_name, FUNCTION, nil, function_return_type)
	if output_code == NAME_COLLISION {
		return NewResult(MOD_ERR, fmt.Sprintf("Invalid redefinition of function: %s.", function_name))
	}

	func_symbol, _ := c.current_scope.scope.LookupSymbol(function_name)

	// initialize the function symbol scope
	func_symbol.scope = &SymbolTable{}
	func_symbol.value = &AtomicValue{-1, []*AtomicValue{}, nil}

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

	function_params, result := c.compile_param_list_node(parameters, found_vararg, IT_PARAM)
	if result.module_cat == MOD_ERR {
		return result
	}

	// the body could be omitted if it is simply a function declaration and not a definition
	var function_body []IRBlock
	if function_signature.Right != nil {
		body := function_signature.Right.(*sage.BlockNode)
		function_body, result = c.compile_block_node(body, "entry")
		if result.module_cat == MOD_ERR {
			return result
		}
	}

	ret_type := func_symbol.sage_datatype_to_llvm()
	if ret_type == "void" {
		implicit_return := IRAtom{
			irtype:           "void",
			instruction_type: IT_RET_STMT,
		}

		function_body[0].Body = append(function_body[0].Body, implicit_return)
	}

	// Leave nested function scope
	c.current_scope = previous_scope

	// generate ir
	ir := IRFunc{
		name:         function_name,
		return_type:  ret_type,
		parameters:   function_params,
		calling_conv: "NOCONV",
		attribute:    "NOATTRIBUTE",
		body:         function_body,
		is_vararg:    *found_vararg,
	}

	c.ir_module.FuncDefs = append(c.ir_module.FuncDefs, ir)

	return NewResult(MOD_NONE, "")
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
	function_signature_parameters := symbol.value.result_value.([]*AtomicValue)
	var signature_ir_type string
	for index, parameter := range parameters.Children {
		// there is more freedom on what values can be passed into a function, theoretically any kind of primary or expression could be passed as a parameter
		instructions, result := c.compile_primary_node(parameter)
		if result.module_cat != MOD_NONE {
			return retval, result
		}

		resultant_inst := instructions[len(instructions)-1]
		register := resultant_inst.ResultRegister()
		irtype, inst_type := resultant_inst.TypeInfo()

		if inst_type != IT_STRLITERAL {
			retval = append(retval, instructions...)
		}

		datatype := function_signature_parameters[index].datatype
		signature_ir_type = sage_to_llvm_type(datatype, -1)

		last_param := index == len(parameters.Children)-1

		deref_type, _ := irtype_dereferenced_type(irtype)
		var final_irtype string = irtype

		if signature_ir_type != "..." {
			if signature_ir_type == irtype {
				final_irtype = irtype
			} else if signature_ir_type == deref_type {
				final_irtype = deref_type
				register = fmt.Sprintf("getelementptr (%s, %s* %s, i32 0, i32 0)", irtype, irtype, register)
			} else {
				return retval, NewResult(MOD_ERR, fmt.Sprintf("Mismatching types found in function %s call parameter.", function_name))
			}
		}

		param_ir := IRAtom{
			name:             register,
			irtype:           final_irtype,
			instruction_type: IT_PARAM,
			is_last_param:    last_param,
		}

		ir_parameters = append(ir_parameters, param_ir)
	}

	// allocate function call result to a register
	result_reg := symbol.NewRegister()

	// get the function's return type
	return_type := symbol.sage_datatype_to_llvm()

	ir := IRFuncCall{
		tail:            false,
		calling_conv:    "NOCONV",
		return_type:     return_type,
		parameter_types: symbol.parameter_signature,
		name:            function_name,
		result_register: result_reg,
		parameters:      ir_parameters,
	}
	retval = append(retval, ir)

	return retval, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_struct_node(node sage.ParseNode) *CompilationResult {
	return NewResult(MOD_ERR, "")
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
		store_ident := "%" + binary.Left.Get_token().Lexeme

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
			name:             "%" + store_ident,
			value:            rhs_ir[len(rhs_ir)-1].ResultRegister(),
			irtype:           symbol.sage_datatype_to_llvm(),
			instruction_type: IT_STORE,
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
			name:             "%" + assign_name,
			irtype:           llvm_type,
			instruction_type: IT_INIT,
		}
		retval = append(retval, stack_allocate)

		store_instruction := IRAtom{
			name:             "%" + assign_name,
			value:            rhs_inst[len(rhs_inst)-1].ResultRegister(),
			irtype:           llvm_type,
			instruction_type: IT_STORE,
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
		cat := c.compile_string_node(node.(*sage.UnaryNode))
		ir := c.pop() // get string node ir that was just compiled and exists in the buffer after compile_string_node finishes
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

	stack_allocate := &IRAtom{
		name:             var_name,
		irtype:           assignment_type,
		instruction_type: IT_INIT,
	}

	return stack_allocate, NewResult(MOD_NONE, "")
}

func (c *SageCompiler) compile_var_ref_node(node *sage.UnaryNode) (*IRAtom, *CompilationResult) {
	var_name := node.Get_token().Lexeme

	symbol, output := c.current_scope.scope.LookupSymbol(var_name)
	if output == NAME_UNDEFINED {
		return nil, NewResult(MOD_ERR, fmt.Sprintf("Invalid reference to variable: %s.", var_name))
	}

	reg := symbol.NewRegister()

	ir_type := symbol.sage_datatype_to_llvm()

	load_inst := &IRAtom{
		name:             "%" + var_name,
		irtype:           ir_type,
		instruction_type: IT_REF,
		result_register:  reg,
	}

	return load_inst, NewResult(MOD_NONE, "")
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

	case sage.VARARG:
		return VARARG, false

	case sage.TYPE:
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
		INT:    "i32",
		BOOL:   "i1",
		I16:    "i16",
		I32:    "i32",
		I64:    "i64",
		F32:    "float32",
		F64:    "float64",
		CHAR:   "i8",
		VOID:   "void",
		VARARG: "...",

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

func irtype_dereferenced_type(irtype string) (string, bool) {
	// handle pointer type derefence first
	if irtype[len(irtype)-1] == '*' {
		delimited := strings.Split(irtype, "")

		inner_irtype := delimited[0]
		return inner_irtype, true

	} else if strings.Contains(irtype, "x") && strings.Contains(irtype, "[") && strings.Contains(irtype, "]") {
		delimited := strings.Split(irtype, " ") // generated llvm ir array types will always have spaces in between the tokens

		inner_irtype := delimited[3]
		return fmt.Sprintf("%s*", inner_irtype), true
	}

	// otherwise the type cannot produce a dereferenced type
	return "", false
}

func set_datalayouts(platform string, architecture string, bitsize string) (datalayout string, triple string, success bool) {
	datalayout_map := map[string]string{
		"LINUX:X86:64":   "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
		"LINUX:X86:32":   "e-m:e-p:32:32-f64:32:64-f80:32-n8:16:32-S128",
		"DARWIN:X86:64":  "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
		"DARWIN:ARM:64":  "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
		"WINDOWS:X64:64": "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
		"WINDOWS:X86:32": "e-m:x-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32",
	}

	input := fmt.Sprintf("%s:%s:%s", remove_wrapping_quotes(platform), remove_wrapping_quotes(architecture), bitsize)
	datalayout, exists := datalayout_map[input]
	if !exists {
		return "", "", false
	}

	platform_triple_map := map[string]string{
		"LINUX:X86:64":   "x86_64-pc-linux-gnu",
		"LINUX:X86:32":   "i686-pc-linux-gnu",
		"DARWIN:X86:64":  "x86_64-apple-darwin",
		"DARWIN:ARM:64":  "arm64-apple-darwin",
		"WINDOWS:X64:64": "x86_64-pc-windows-msvc",
		"WINDOWS:X86:32": "i686-pc-windows-msvc",
	}

	triple, exists = platform_triple_map[input]
	if !exists {
		return "", "", false
	}

	return datalayout, triple, true
}

func remove_wrapping_quotes(input_string string) string {
	return input_string[1 : len(input_string)-1]
}

func sage_literal_to_llvm_literal(sage_literal string) string {
	sage_substrings := []string{`\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`, `\'`, `\"`, `\?`, `\0`}
	llvm_substrings := []string{`\07`, `\08`, `\0C`, `\0A`, `\0D`, `\09`, `\0B`, `\5C`, `\27`, `\22`, `\3F`, `\00`}

	var result string = sage_literal
	for i := range 12 {
		if !strings.Contains(sage_literal, sage_substrings[i]) {
			continue
		}

		result = strings.ReplaceAll(sage_literal, sage_substrings[i], llvm_substrings[i])
	}

	result = result[0 : len(result)-1]
	result += `\00"`

	return result
}
