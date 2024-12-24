package sage

type Visit int

const (
	FUNCDEF Visit = iota
	FUNCCALL
	STRUCT
	BLOCK // might not need
	IF
)

type NodeVisitor interface {
	CurrentlyVisiting() Visit
	ResultType() string
	ResultValue() Value
}

type FuncDefVisitor struct {
	Parameter_amount     int
	Current_param        int
	function_return_type string
}

func (v *FuncDefVisitor) CurrentlyVisiting() Visit {
	return FUNCDEF
}

func (v *FuncDefVisitor) ResultType() string {
	return v.function_return_type
}

func (v *FuncDefVisitor) ResultValue() Value {
	return nil
}

type FuncCallVisitor struct {
	Parameter_amount     int
	Current_param        int
	function_return_type string
}

func (v *FuncCallVisitor) CurrentlyVisiting() Visit {
	return FUNCCALL
}

func (v *FuncCallVisitor) ResultType() string {
	return v.function_return_type
}

func (v *FuncCallVisitor) ResultValue() Value {
	return nil
}
