package sage

import (
	"github.com/google/uuid"
	lexer "sage/internal/lexer"
)

type SymbolTableEntry struct {
	erroneous_entry bool
	entry_value     Result
}

func NewEntryError() *SymbolTableEntry {
	return &SymbolTableEntry{
		erroneous_entry: true,
	}
}

func NewEntry() *SymbolTableEntry {
	return &SymbolTableEntry{
		erroneous_entry: false,
		entry_value:     nil,
	}
}

func NewEntryWith(error bool, value Result, table *SymbolTable) *SymbolTableEntry {
	return &SymbolTableEntry{
		erroneous_entry: error,
		entry_value:     value,
	}
}

type SymbolTable struct {
	table     map[string]*SymbolTableEntry
	registers *lexer.Queue[lexer.RegisterPair]
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		table: map[string]*SymbolTableEntry{},
	}
}

func (st *SymbolTable) GetRegister() lexer.RegisterPair {
	return *st.registers.Pop()
}

func (st *SymbolTable) UpdateRegisters(value string, name string) {
	st.registers.Stack(lexer.NewRegisterPair(name, value))
}

func (st *SymbolTable) NewRegisterFor(value string) string {
	register_id := uuid.New().String()[:5] // unique 5 character string id
	st.registers.Stack(lexer.NewRegisterPair(register_id, value))
	return register_id
}

func (st *SymbolTable) NewEntry(name string) *SymbolTableEntry {
	_, exists := st.table[name]
	if exists {
		return nil
	}

	st.table[name] = NewEntry()
	return st.table[name]
}

func (st *SymbolTable) GetEntryValue(name string) Result {
	// not meant for accessing structs via list node lexeme list
	entry, exists := st.table[name]
	if !exists {
		return nil
	}

	return entry.entry_value
}

func (st *SymbolTable) GetEntryNamed(name string) (*SymbolTableEntry, bool) {
	entry, exists := st.table[name]
	return entry, exists
}

func (st *SymbolTable) SetEntryNamed(name string, value Result) bool {
	_, exists := st.table[name]
	if !exists {
		return false
	}

	st.table[name].entry_value = value
	return true
}
