package sage

type VisitType int

const (
	FUNCDEF VisitType = iota
	FUNCCALL
	STRUCT
	BLOCK // might not need
	IF
	FOR
	WHILE
	PROGRAMROOT
	EXPRESSION
)

type NodeVisitor struct {
	scope_name        string
	parameter_amount  int
	current_param     int
	scope_return_type string
	visit_type        VisitType
	result_type       string
	table             *SymbolTable
	parent_visitor    *NodeVisitor
}

func NewRootVisitor(global_table *SymbolTable) *NodeVisitor {
	return &NodeVisitor{
		scope_name: "ROOT",
		visit_type: PROGRAMROOT,
		table:      global_table,
	}
}

func NewVisitor(visit_type VisitType, name string, parent *NodeVisitor) *NodeVisitor {
	return &NodeVisitor{
		scope_name:     name,
		visit_type:     visit_type,
		table:          NewSymbolTable(),
		parent_visitor: parent,
	}
}

func (v *NodeVisitor) FindVariableNamed(name string) (*SymbolTableEntry, bool) {
	return v.table.GetEntryNamed(name)
}
