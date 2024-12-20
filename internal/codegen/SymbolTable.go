package sage

type SymbolTable struct {
	table map[string]any
}

func (st *SymbolTable) NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		table: map[string]any{},
	}
}
