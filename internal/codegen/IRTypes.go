package sage

import (
	"fmt"
	"strings"
)

type IRInstructionProtocol interface {
	ToLLVM() string
}

type InstructionList interface{}

type VarInstructionType int

const (
	INIT VarInstructionType = iota
	REF
	PARAM
	RET_STMT
	ASSIGN
)

// TODO: if statement IR
// TODO: while statement IR
// TODO: for statement IR
// TODO: range statement IR

type IRModule struct {
	source_filename   string
	target_datalayout string
	target_triple     string
	FuncDecs          []IRFunc
	FuncDefs          []IRFunc
	Globals           []IRGlobal
	Structs           []IRStruct
	GLOBAL_TABLE      *SymbolTable
}

func NewIRModule(filename string, globals *SymbolTable) *IRModule {
	return &IRModule{
		source_filename:   filename,
		target_datalayout: "",
		target_triple:     "",
		FuncDecs:          []IRFunc{},
		FuncDefs:          []IRFunc{},
		Globals:           []IRGlobal{},
		Structs:           []IRStruct{},
	}
}

func (ir *IRModule) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(fmt.Sprintf("source_filename = \"%s\"\n", ir.source_filename))
	builder.WriteString(fmt.Sprintf("target datalayout = \"%s\"\n", ir.target_datalayout))
	builder.WriteString(fmt.Sprintf("target triple = \"%s\"\n\n", ir.target_triple))

	for _, declaration := range ir.FuncDecs {
		builder.WriteString(declaration.ToLLVM())
		builder.WriteString("\n")
	}

	for _, definition := range ir.FuncDefs {
		builder.WriteString(definition.ToLLVM())
		builder.WriteString("\n\n")
	}

	for _, global := range ir.Globals {
		builder.WriteString(global.ToLLVM())
		builder.WriteString("\n")
	}

	for _, construct := range ir.Structs {
		builder.WriteString(construct.ToLLVM())
		builder.WriteString("\n")
	}

	return builder.String()
}

type IRFunc struct {
	name         string
	return_type  string
	parameters   []IRAtom
	calling_conv string
	attribute    string
	body         []IRBlock
}

func (ir *IRFunc) ToLLVM() string {
	var builder strings.Builder
	var start_keyword string
	is_definition := len(ir.body) != 0
	if is_definition {
		start_keyword = "define "
	} else {
		start_keyword = "declare "
	}

	builder.WriteString(start_keyword)

	if ir.calling_conv != "NOCONV" {
		builder.WriteString(ir.calling_conv + " ")
	}

	builder.WriteString(fmt.Sprintf("%s ", ir.return_type))
	builder.WriteString(fmt.Sprintf("@%s(", ir.name))

	ir.parameters[len(ir.parameters)-1].is_last_param = true
	for _, param := range ir.parameters {
		builder.WriteString(param.ToLLVM())
	}
	builder.WriteString(")")

	if ir.attribute != "NOATTRIBUTE" {
		builder.WriteString(fmt.Sprintf(" %s", ir.attribute))
	}

	if is_definition {
		builder.WriteString(" {\n")
		for _, block := range ir.body {
			builder.WriteString(block.ToLLVM())
			builder.WriteString("\n")
		}
		builder.WriteString("}")
	}

	return builder.String()
}

type IRFuncCall struct {
	tail         bool
	calling_conv string
	return_type  string
	name         string
	parameters   []IRAtom
	table        *SymbolTableEntry // this should point to the same table that was passed into the entry for the IRFuncDef as well
}

func (ir *IRFuncCall) ToLLVM() string {
	var builder strings.Builder
	if ir.return_type != "void" {
		result_name := ir.table.NewRegisterFor(ir.name)
		builder.WriteString(fmt.Sprintf("%%%s = ", result_name))
	}

	if ir.tail {
		builder.WriteString("tail ")
	}

	builder.WriteString("call ")

	if ir.calling_conv != "NOCONV" {
		builder.WriteString(fmt.Sprintf("%s ", ir.calling_conv))
	}

	builder.WriteString(fmt.Sprintf("%s @%s(", ir.return_type, ir.name))
	ir.parameters[len(ir.parameters)-1].is_last_param = true
	for _, atom := range ir.parameters {
		builder.WriteString(atom.ToLLVM())
	}
	builder.WriteString(")")

	return ""
}

type IRGlobal struct {
	name   string
	irtype string
	value  string
}

func (ir *IRGlobal) ToLLVM() string {
	return fmt.Sprintf("@%s = global %s, %s", ir.name, ir.irtype, ir.value)
}

type IRBlock struct {
	Name string
	Body []IRInstructionProtocol
}

func (ir *IRBlock) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(fmt.Sprintf("%s:\n", ir.Name))
	for _, construct := range ir.Body {
		construct_ir := construct.ToLLVM()
		builder.WriteString("\t" + construct_ir)
	}
	return builder.String()
}

type IRAtom struct {
	name             string
	irtype           string
	value            string
	instruction_type VarInstructionType
	is_last_param    bool
	table            *SymbolTableEntry // corresponding table entry also stores all associated register names in the order they were declared
}

func (ir *IRAtom) ToLLVM() string {
	var builder strings.Builder
	switch ir.instruction_type {
	case INIT:
		builder.WriteString(fmt.Sprintf("%%%s = alloca %s", ir.name, ir.irtype))

	case REF:
		// NOTE: the logic around this might need to be different
		result_reg := ir.table.NewRegisterFor(ir.name)
		builder.WriteString(fmt.Sprintf("%%%s = load %s, %s* %%%s", result_reg, ir.irtype, ir.irtype, ir.name))

	case PARAM:
		if ir.is_last_param {
			builder.WriteString(fmt.Sprintf("%s %%%s", ir.irtype, ir.name))
		} else {
			builder.WriteString(fmt.Sprintf("%s %%%s, ", ir.irtype, ir.name))
		}

	case RET_STMT:
		if ir.name != "NONAME" {
			builder.WriteString(fmt.Sprintf("ret %s %%%s", ir.irtype, ir.name))
		} else {
			builder.WriteString(fmt.Sprintf("ret void"))
		}

	case ASSIGN:
		builder.WriteString(fmt.Sprintf("store %s %%%s, %s* %%%s", ir.irtype, ir.value, ir.irtype, ir.name))

	}
	return builder.String()
}

type IRFunctionType struct {
	name    string
	irtype  string
	irtypes []string
}

func (ir *IRFunctionType) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(fmt.Sprintf("%%%s = type %s (", ir.name, ir.irtype))

	params := ""
	for _, irtype := range ir.irtypes {
		params += fmt.Sprintf("%s, ", irtype)
	}
	params = params[:len(params)-2]
	builder.WriteString(params)
	builder.WriteString(")")

	return builder.String()
}

type IRStruct struct {
	name            string
	attribute_types []string
	packed          bool
}

func (ir *IRStruct) ToLLVM() string {
	var builder strings.Builder
	var lbracket_sym = "{"
	var rbracket_sym = "}"
	if ir.packed {
		lbracket_sym = "<{"
		rbracket_sym = "}>"
	}

	builder.WriteString(fmt.Sprintf("%%%s = type %s\n", ir.name, lbracket_sym))

	var attribute_str string
	for _, irtype := range ir.attribute_types {
		attribute_str += fmt.Sprintf("%s,\n", irtype)
	}
	attribute_str = attribute_str[:len(attribute_str)-2]
	attribute_str += "\n"
	builder.WriteString(attribute_str)
	builder.WriteString(rbracket_sym)

	return builder.String()
}

type IRExpression struct {
	operation_name string
	irtype         string
	operand1       string
	operand2       string
	table          *SymbolTableEntry
}

func (ir *IRExpression) ToLLVM() string {
	var builder strings.Builder
	result_name := ir.table.NewRegisterFor(fmt.Sprintf("%s %s %s", ir.operand1, ir.operation_name, ir.operand2))
	builder.WriteString(fmt.Sprintf("%%%s = %s %s %s, %s", result_name, ir.operation_name, ir.irtype, ir.operand1, ir.operand2))
	return builder.String()
}
