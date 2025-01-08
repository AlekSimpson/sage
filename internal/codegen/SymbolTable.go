package sage

import "strings"
import "fmt"

type AST_TYPE int

const (
	VARIABLE AST_TYPE = iota
	CONSTANT
	FUNCTION
	PARAMETER
	STRUCT
	IF
	FOR
	PROGRAMROOT
	EXPRESSION
)

func (t AST_TYPE) String() string {
	typemap := map[AST_TYPE]string{
		VARIABLE:    "VARIABLE",
		CONSTANT:    "CONSTANT",
		FUNCTION:    "FUNCTION",
		PARAMETER:   "PARAMETER",
		STRUCT:      "STRUCT",
		IF:          "IF",
		FOR:         "FOR",
		PROGRAMROOT: "PROGRAMROOT",
		EXPRESSION:  "EXPRESSION",
	}

	return typemap[t]
}

type SAGE_DATATYPE int

const (
	INT SAGE_DATATYPE = iota
	BOOL
	I16
	I32
	I64
	F32
	F64
	CHAR
	VOID

	ARRAY_I16
	ARRAY_I32
	ARRAY_I64
	ARRAY_F32
	ARRAY_F64
	ARRAY_CHAR
	ARRAY_BOOL

	POINTER_I16
	POINTER_I32
	POINTER_I64
	POINTER_F32
	POINTER_F64
	POINTER_CHAR
	POINTER_BOOL

	STRUCT_TYPE
)

func (t SAGE_DATATYPE) String() string {
	typemap := map[SAGE_DATATYPE]string{
		INT:  "INT",
		BOOL: "BOOL",
		I16:  "I16",
		I32:  "I32",
		I64:  "I64",
		F32:  "F32",
		F64:  "F64",
		CHAR: "CHAR",
		VOID: "VOID",

		ARRAY_I16:  "ARRAY_I16",
		ARRAY_I32:  "ARRAY_I32",
		ARRAY_I64:  "ARRAY_I64",
		ARRAY_F32:  "ARRAY_F32",
		ARRAY_F64:  "ARRAY_F64",
		ARRAY_CHAR: "ARRAY_CHAR",
		ARRAY_BOOL: "ARRAY_BOOL",

		POINTER_I16:  "POINTER_I16",
		POINTER_I32:  "POINTER_I32",
		POINTER_I64:  "POINTER_I64",
		POINTER_F32:  "POINTER_F32",
		POINTER_F64:  "POINTER_F64",
		POINTER_CHAR: "POINTER_CHAR",
		POINTER_BOOL: "POINTER_BOOL",

		STRUCT_TYPE: "STRUCT_TYPE",
	}

	return typemap[t]
}

type AtomicValue struct {
	datatype     SAGE_DATATYPE
	result_value any
	struct_map   map[string]any
}

type Symbol struct {
	name             string
	ast_type         AST_TYPE
	registers        string
	value            *AtomicValue
	sage_datatype    SAGE_DATATYPE
	scope            *SymbolTable
	parameter_amount int
	parameter_index  int
	array_length     int
}

type SymbolTable map[string]*Symbol

type TableOutput int

const (
	SUCCESS TableOutput = iota
	NAME_COLLISION
	NAME_UNDEFINED
)

func (t *SymbolTable) AddSymbol(name string, ast AST_TYPE, value *AtomicValue, datatype SAGE_DATATYPE) TableOutput {
	_, exists := (*t)[name]
	if exists {
		return NAME_COLLISION
	}

	(*t)[name] = &Symbol{name, ast, "", value, datatype, nil, 0, 0, 0}
	return SUCCESS
}

func (t *SymbolTable) LookupSymbol(name string) (*Symbol, TableOutput) {
	symbol, exists := (*t)[name]
	if !exists {
		return nil, NAME_UNDEFINED
	}

	return symbol, SUCCESS
}

func (t *Symbol) NewRegister() string {
	reg_name := CreateInternalName()
	return reg_name
}

func (t *Symbol) SetValue(datatype SAGE_DATATYPE, value any) {
	t.value.datatype = datatype
	t.value.result_value = value
}

func (t *Symbol) FieldWrite(fieldname string, value any) bool {
	_, exists := t.value.struct_map[fieldname]
	if !exists {
		return false
	}

	t.value.struct_map[fieldname] = value
	return true
}

func (t *Symbol) FieldRead(fieldname string) any {
	value, exists := t.value.struct_map[fieldname]
	if !exists {
		return nil
	}

	return value
}

func (s *Symbol) sage_datatype_to_llvm() string {
	return sage_to_llvm_type(s.sage_datatype, s.array_length)
}

func (s *Symbol) ShowTables() string {
	var builder strings.Builder
	builder.WriteString("---[Sage Symbol State]-----\n")
	builder.WriteString(fmt.Sprintf("NAME: %s\n", s.name))
	builder.WriteString(fmt.Sprintf("AST_TYPE: %s\n", s.ast_type))
	builder.WriteString(fmt.Sprintf("REGISTER: %s\n", s.registers))
	if s.value != nil {
		builder.WriteString(fmt.Sprintf("VALUE: %+v\n", s.value.result_value))
	}
	builder.WriteString(fmt.Sprintf("SAGE_DT: %s\n", s.sage_datatype))
	builder.WriteString(fmt.Sprintf("PARAMETER AMOUNT: %d\n", s.parameter_amount))
	builder.WriteString(fmt.Sprintf("PARAMETER INDEX: %d\n", s.parameter_index))
	builder.WriteString(fmt.Sprintf("ARRAY LENGTH: %d\n", s.array_length))

	if s.scope != nil {
		builder.WriteString(fmt.Sprintf("SCOPE: \n\t%s\n", ShowSymbolTable(*s.scope)))
	}

	return builder.String()
}

func ShowSymbolTable(table SymbolTable) string {
	var builder strings.Builder
	for key, value := range table {
		builder.WriteString(fmt.Sprintf("%s : %+v\n", key, value.value))
	}

	return builder.String()
}
