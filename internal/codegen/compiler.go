package sage

import (
	// "fmt"
	"sage/internal/parser"
)

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

	compile_code(parsetree, code_module)
}

func compile_code(parsetree sage.ParseNode, ir_module *IRModule) {
	interpreter := NewInterpreter(ir_module.GLOBAL_TABLE)

	// loop through root node chilren
	// generate IR nodes for each child node
	program_root_node := parsetree.(*sage.BlockNode)
	for _, node := range program_root_node.Children {
		switch node.Get_true_nodetype() {
		case sage.BINARY:
			// NEED
		case sage.TRINARY:
		case sage.UNARY:
		case sage.NUMBER:
			// NEED
		case sage.STRING:
			// NEED
		case sage.IDENTIFIER:
		case sage.KEYWORD:
		case sage.BLOCK:
		case sage.CODE_BLOCK:
		case sage.PARAM_LIST:
		case sage.FUNCDEF:
			// NEED
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
		case sage.VAR_REF:
		case sage.COMPILE_TIME_EXECUTE:
			unary_node := node.(*sage.UnaryNode)
			block_node := unary_node.Get_child_node().(*sage.BlockNode)
			for _, childnode := range block_node.Children {
				interpreter.interpret(childnode)
				// NOTE: in the future we may want or need to expand this out but for now all we really need compile time execute to do is update the symbol table
			}
		}
	}
}
