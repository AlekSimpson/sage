package sage

type SymbolTable struct {
	table map[string]any
}

func (st *SymbolTable) NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		table: map[string]any{},
	}
}

func (st *SymbolTable) GetEntry(name string) any {
	entry, exists := st.table[name]
	if !exists {
		return nil
	}

	return entry
}
