package sage

type SymbolTableEntry struct {
	erroneous_entry bool
	entry_value     any
	entry_table     *SymbolTable
}

func NewEntryError() SymbolTableEntry {
	return SymbolTableEntry{
		erroneous_entry: true,
	}
}

type SymbolTable struct {
	table map[string]SymbolTableEntry
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		table: map[string]SymbolTableEntry{},
	}
}

func (st *SymbolTable) NewEntry(name string) bool {
	_, exists := st.table[name]
	if exists {
		return false
	}

	st.table[name] = SymbolTableEntry{false, nil, nil}
	return true
}

func (st *SymbolTable) GetEntry(name string) SymbolTableEntry {
	entry, exists := st.table[name]
	if !exists {
		return NewEntryError()
	}

	return entry
}

func (st *SymbolTable) SetEntry(name string, value any) bool {
	prev, exists := st.table[name]
	if !exists {
		return false
	}

	st.table[name] = SymbolTableEntry{false, value, prev.entry_table}
	return true
}
