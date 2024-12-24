package sage

import (
	"github.com/google/uuid"
	lexer "sage/internal/lexer"
	"sage/internal/parser"
)

type SymbolTableEntry struct {
	erroneous_entry bool
	entry_value     Result
	entry_table     *SymbolTable
	registers       *lexer.Queue[lexer.RegisterPair]
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
		entry_table:     nil,
		registers:       lexer.NewQueue([]lexer.RegisterPair{}, lexer.NewRegisterPair("", "")),
	}
}

func NewEntryWith(error bool, value Result, table *SymbolTable) *SymbolTableEntry {
	return &SymbolTableEntry{
		erroneous_entry: error,
		entry_value:     value,
		entry_table:     table,
		registers:       lexer.NewQueue([]lexer.RegisterPair{}, lexer.NewRegisterPair("", "")),
	}
}

func (entry *SymbolTableEntry) GetRegister() lexer.RegisterPair {
	return entry.registers.Pop()
}

func (entry *SymbolTableEntry) UpdateRegisters(value string, name string) {
	entry.registers.Stack(lexer.NewRegisterPair(name, value))
}

func (entry *SymbolTableEntry) NewRegisterFor(value string) string {
	register_id := uuid.New().String()[:5] // unique 5 character string id
	entry.registers.Stack(lexer.NewRegisterPair(register_id, value))
	return register_id
}

type SymbolTable struct {
	table map[string]*SymbolTableEntry
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		table: map[string]*SymbolTableEntry{},
	}
}

func (st *SymbolTable) NewEntry(name string) bool {
	_, exists := st.table[name]
	if exists {
		return false
	}

	st.table[name] = NewEntry()
	return true
}

func (st *SymbolTable) GetEntry(name_node sage.ParseNode) *SymbolTableEntry {
	if name_node.Get_true_nodetype() == sage.IDENTIFIER {
		name := name_node.Get_token().Lexeme
		return st.GetEntryNamed(name)
	}

	list_node := name_node.(*sage.ListNode)
	return st.get_entry_nested(list_node)
}

func (st *SymbolTable) GetEntryValue(name string) Result {
	// not meant for accessing structs via list node lexeme list
	entry, exists := st.table[name]
	if !exists {
		return nil
	}

	return entry.entry_value
}

func (st *SymbolTable) get_entry_nested(name_node *sage.ListNode) *SymbolTableEntry {
	var current_table *SymbolTable = st
	var lastname string
	for _, name := range name_node.Lexemes {
		entry := current_table.GetEntryNamed(name)
		if entry.entry_table == nil {
			lastname = name
			break
		}
		current_table = entry.entry_table
	}

	return current_table.GetEntryNamed(lastname)
}

func (st *SymbolTable) GetEntryNamed(name string) *SymbolTableEntry {
	entry, exists := st.table[name]
	if !exists {
		return NewEntryError()
	}

	return entry
}

func (st *SymbolTable) SetEntry(name_node sage.ParseNode, value Result) bool {
	if name_node.Get_true_nodetype() == sage.IDENTIFIER {
		name := name_node.Get_token().Lexeme
		return st.SetEntryNamed(name, value)
	}

	list_node := name_node.(*sage.ListNode)
	return st.set_entry_nested(list_node, value)
}

func (st *SymbolTable) SetEntryNamed(name string, value Result) bool {
	prev, exists := st.table[name]
	if !exists {
		return false
	}

	st.table[name] = NewEntryWith(false, value, prev.entry_table)
	return true
}

func (st *SymbolTable) set_entry_nested(name_node *sage.ListNode, value Result) bool {
	var current_table *SymbolTable = st
	var lastname string
	for _, name := range name_node.Lexemes {
		entry := current_table.GetEntryNamed(name)
		if entry.entry_table == nil {
			lastname = name
			break
		}
		current_table = entry.entry_table
	}

	return current_table.SetEntryNamed(lastname, value)
}

func (st *SymbolTable) Insert(name string, entry *SymbolTableEntry) {
	st.table[name] = entry
}

func (st *SymbolTable) InsertValue(name string, entry Result) {
	st.table[name].entry_value = entry
}
