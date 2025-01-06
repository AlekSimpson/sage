package sage

import (
	"fmt"
	"strings"
)

type IRInstructionProtocol interface {
	ToLLVM() string
	ResultRegister() string
}

type InstructionList interface{}

type VarInstructionType int

const (
	INIT VarInstructionType = iota
	REF
	PARAM
	RET_STMT
	STORE
	GLOBAL
)

// TODO: range statement IR

type IRModule struct {
	source_filename   string
	target_datalayout string
	target_triple     string
	FuncDecs          []IRFunc
	FuncDefs          []IRFunc
	Globals           []IRInstructionProtocol
	Structs           []IRStruct
}

func NewIRModule(filename string) *IRModule {
	return &IRModule{
		source_filename:   filename,
		target_datalayout: "",
		target_triple:     "",
		FuncDecs:          []IRFunc{},
		FuncDefs:          []IRFunc{},
		Globals:           []IRInstructionProtocol{},
		Structs:           []IRStruct{},
	}
}

func (ir IRModule) ToLLVM() string {
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

func (ir IRModule) ResultRegister() string {
	return ""
}

type IRFunc struct {
	name         string
	return_type  string
	parameters   []IRAtom
	calling_conv string
	attribute    string
	body         []IRBlock
	is_vararg    bool
}

func (ir IRFunc) ToLLVM() string {
	var builder strings.Builder
	is_definition := len(ir.body) != 0

	builder.WriteString("define ")

	if ir.calling_conv != "NOCONV" {
		builder.WriteString(ir.calling_conv + " ")
	}

	builder.WriteString(fmt.Sprintf("%s ", ir.return_type))
	builder.WriteString(fmt.Sprintf("@%s(", ir.name))

	ir.parameters[len(ir.parameters)-1].is_last_param = true
	for _, param := range ir.parameters {
		builder.WriteString(param.ToLLVM())
	}
	if ir.is_vararg {
		builder.WriteString(", ...")
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

func (ir IRFunc) ResultRegister() string {
	return ""
}

type IRFuncCall struct {
	tail            bool
	calling_conv    string
	return_type     string
	name            string
	result_register string
	parameters      []IRInstructionProtocol
}

func (ir IRFuncCall) ToLLVM() string {
	var builder strings.Builder
	if ir.return_type != "void" {
		builder.WriteString(fmt.Sprintf("%%%s = ", ir.result_register))
	}

	if ir.tail {
		builder.WriteString("tail ")
	}

	builder.WriteString("call ")

	if ir.calling_conv != "NOCONV" {
		builder.WriteString(fmt.Sprintf("%s ", ir.calling_conv))
	}

	builder.WriteString(fmt.Sprintf("%s @%s(", ir.return_type, ir.name))
	// (might not need?) ir.parameters[len(ir.parameters)-1].is_last_param = true
	// FIX: this will probably generate invalid IR but I want to see how it geenrates to fix it first
	for _, inst := range ir.parameters {
		builder.WriteString(inst.ToLLVM())
	}
	builder.WriteString(")")

	return ""
}

func (ir IRFuncCall) ResultRegister() string {
	return ir.result_register
}

type IRBlock struct {
	Name string
	Body []IRInstructionProtocol
}

func (ir IRBlock) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(fmt.Sprintf("%s:\n", ir.Name))
	for _, construct := range ir.Body {
		construct_ir := construct.ToLLVM()
		builder.WriteString("\t" + construct_ir)
	}
	return builder.String()
}

func (ir IRBlock) ResultRegister() string {
	return ""
}

type IRAtom struct {
	name             string
	irtype           string
	value            string
	instruction_type VarInstructionType
	is_last_param    bool
	result_register  string
}

func (ir IRAtom) ToLLVM() string {
	var builder strings.Builder
	switch ir.instruction_type {
	case INIT:
		builder.WriteString(fmt.Sprintf("%%%s = alloca %s", ir.name, ir.irtype))

	case REF:
		// NOTE: the logic around this might need to be different
		builder.WriteString(fmt.Sprintf("%%%s = load %s, %s* %%%s", ir.result_register, ir.irtype, ir.irtype, ir.name))

	case PARAM:
		if ir.is_last_param {
			builder.WriteString(fmt.Sprintf("%s %%%s", ir.irtype, ir.name))
		} else {
			builder.WriteString(fmt.Sprintf("%s %%%s, ", ir.irtype, ir.name))
		}

	case RET_STMT:
		if ir.name != "NONAME" {
			builder.WriteString(fmt.Sprintf("ret %s %%%s", ir.irtype, ir.value))
		} else {
			builder.WriteString("ret void")
		}

	case STORE:
		builder.WriteString(fmt.Sprintf("store %s %%%s, %s* %%%s", ir.irtype, ir.value, ir.irtype, ir.name))

	case GLOBAL:
		builder.WriteString(fmt.Sprintf("@%s = global %s, %s", ir.name, ir.irtype, ir.value))

	}
	return builder.String()
}

func (ir IRAtom) ResultRegister() string {
	return ir.result_register
}

type IRFunctionType struct {
	name    string
	irtype  string
	irtypes []string
}

func (ir IRFunctionType) ToLLVM() string {
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

func (ir IRFunctionType) ResultRegister() string {
	return ""
}

type IRStruct struct {
	name            string
	attribute_types []string
	packed          bool
}

func (ir IRStruct) ToLLVM() string {
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

func (ir IRStruct) ResultRegister() string {
	return ""
}

type IRExpression struct {
	expression_id   string
	operation_name  string
	irtype          string
	operand1        string
	operand2        string
	result_register string
}

func (ir IRExpression) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(fmt.Sprintf("%%%s = %s %s %s, %s", ir.result_register, ir.operation_name, ir.irtype, ir.operand1, ir.operand2))
	return builder.String()
}

func (ir IRExpression) ResultRegister() string {
	return ir.result_register
}

type IRWhile struct {
	entry      IRBlock
	condition  IRBlock
	body       IRBlock
	end        IRBlock
	all_blocks []IRBlock
}

func (ir IRWhile) ResultRegister() string {
	return ""
}

func (ir IRWhile) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(ir.entry.ToLLVM())
	builder.WriteString(ir.condition.ToLLVM())
	builder.WriteString(ir.body.ToLLVM())
	builder.WriteString(ir.end.ToLLVM())
	return builder.String()
}

type IRFor struct {
	entry      IRBlock
	condition  IRBlock
	body       IRBlock
	increment  IRBlock
	end        IRBlock
	all_blocks []IRBlock
}

func (ir IRFor) ToLLVM() string {
	var builder strings.Builder
	builder.WriteString(ir.entry.ToLLVM())
	builder.WriteString(ir.condition.ToLLVM())
	builder.WriteString(ir.increment.ToLLVM())
	builder.WriteString(ir.end.ToLLVM())
	return builder.String()
}

func (ir IRFor) ResultRegister() string {
	return ""
}

type IRConditional struct {
	condition_checks []IRBlock
	condition_bodies []IRBlock
	else_body        *IRBlock
	all_blocks       []IRBlock
}

func (ir IRConditional) ToLLVM() string {
	var builder strings.Builder
	for _, checks := range ir.condition_checks {
		builder.WriteString(checks.ToLLVM())
	}
	for _, bodies := range ir.condition_bodies {
		builder.WriteString(bodies.ToLLVM())
	}

	if ir.else_body != nil {
		builder.WriteString(ir.else_body.ToLLVM())
	}
	return builder.String()
}

func (ir IRConditional) ResultRegister() string {
	return ""
}
