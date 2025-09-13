# SageVM Bytecode Reference Manual

## Table of Contents
- [Introduction](#introduction)
- [Register Set](#register-set)
- [Register Data Format](#register-data-format)
- [Instruction Format](#instruction-format)
- [Instruction Set](#instruction-set)
  - [Data Movement Instructions](#data-movement-instructions)
  - [Arithmetic Operations](#arithmetic-operations)
  - [Logical Operations](#logical-operations)
  - [Comparison Operations](#comparison-operations)
  - [Control Flow](#control-flow)
  - [Memory Operations](#memory-operations)
  - [Special Instructions](#special-instructions)
- [Bytecode File Format](#bytecode-file-format)
- [Data Types](#data-types)
- [Error Handling](#error-handling)
- [VM Execution Model](#vm-execution-model)
- [Instruction Encoding Reference](#instruction-encoding-reference)
- [Implementation Status](#implementation-status)

## Introduction

This document describes the bytecode specification for SageVM, a virtual machine designed to support the Sage interpreter. SageVM uses a register-based architecture with 125 registers and supports various data types including integers, floats, and pointers with type tagging.

## Register Set

SageVM provides 125 registers (0-124) with the following allocation:

**Function Parameter Registers (0-5)**
- Used for passing function arguments

**Return Value Registers (6-9)**
- Used for function return values

**Volatile Registers (10-20)**
- Used for temporary values and intermediate results during computation
- Available volatile set: {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}

**System Registers (21-23)**
- Register 21 (LOGIC_REG): Stores boolean/logical operation results
- Register 22 (SYSCALL_REG): Used for system call operations
- Register 23 (STACK_POINTER): Current stack frame pointer

**General Purpose Registers (24-124)**
- Available for general computation and variable storage

## Register Data Format

Each register is a 64-bit value with embedded type information:
- **Upper 32 bits**: Data value (int32, float32 bits, or pointer address)
- **Lower 3 bits**: Type tag (I32_REG=0, F32_REG=1, PTR_REG=2)
- **Bits 3-31**: Unused/reserved

Type checking is performed using `unpack_type()` which extracts the lower 3 bits.

## Instruction Format

Instructions consist of:
- **Opcode**: Identifies the operation type (SageOpCode enum)
- **Operands**: Up to 4 operands packed into a 32-bit value
- **Dereference Map**: Defines how operands should be dereferenced:
  - **0**: Raw value (use operand directly as packed integer)
  - **1**: Register dereference (use operand as register index)
  - **2**: Stack dereference (use operand as stack offset) - *Not currently implemented*
  - **3**: Heap dereference (use operand as heap pointer) - *Not currently implemented*

## Instruction Set

### Data Movement Instructions

**MOV (Opcode: OP_MOV)**
- Format: MOV src, dst
- Operands: 2 (source value, destination register)
- Description: Copy value from source operand to destination register
- Implementation: `registers[dest] = operands[0]`
- Note: Source operand value is dereferenced according to deref_map, destination is always a register index

**LOAD (Opcode: OP_LOAD)**
- Format: LOAD dst, offset
- Operands: 2 (destination register, stack offset)
- Description: Load value from stack at (stack_pointer + offset) into destination register
- Implementation: `registers[dst] = stack[stack_pointer + offset].load()`
- Note: Uses `SageValue.load()` method to convert stack value to register format

**STORE (Opcode: OP_STORE)**
- Format: STORE src, offset
- Operands: 2 (source value, stack offset)
- Description: Store value from source operand to stack at (stack_pointer + offset)
- Implementation: `stack[stack_pointer + offset] = register_to_value(operands[0])`
- Note: Uses `register_to_value()` to convert register format to SageValue for stack storage

### Arithmetic Operations

**ADD (Opcode: OP_ADD)**
- Format: ADD dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer addition: `dst = src1 + src2`
- Implementation: `registers[target_register] = pack_int(unpack_int(operands[1]) + unpack_int(operands[2]))`
- Note: First operand contains the target register index, result is packed as I32_REG

**SUB (Opcode: OP_SUB)**
- Format: SUB dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer subtraction: `dst = src1 - src2`
- Implementation: `registers[target_register] = pack_int(unpack_int(operands[1]) - unpack_int(operands[2]))`
- Note: First operand contains the target register index, result is packed as I32_REG

**MUL (Opcode: OP_MUL)**
- Format: MUL dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer multiplication: `dst = src1 * src2`
- Implementation: `registers[target_register] = pack_int(unpack_int(operands[1]) * unpack_int(operands[2]))`
- Note: First operand contains the target register index, result is packed as I32_REG

**DIV (Opcode: OP_DIV)**
- Format: DIV dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Division: `dst = src1 / src2` (result is stored as float)
- Implementation: `registers[target_register] = pack_float(unpack_int(operands[1]) / unpack_int(operands[2]))`
- Note: Division by zero check performed on `operands[2] == 0`, triggers panic if true

### Logical Operations

**AND (Opcode: OP_AND)**
- Format: AND src1, src2
- Operands: 2 (source1, source2)
- Description: Logical AND operation, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int((value_a && value_b) == 1)`
- Note: Values are unpacked as integers before logical operation

**OR (Opcode: OP_OR)**
- Format: OR src1, src2
- Operands: 2 (source1, source2)
- Description: Logical OR operation, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int((value_a || value_b) == 1)`
- Note: Values are unpacked as integers before logical operation

**NOT (Opcode: OP_NOT)**
- Format: NOT src
- Operands: 1 (source)
- Description: Logical NOT operation, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int(!value)`
- Note: Value is unpacked as integer before logical operation

### Comparison Operations

**EQ (Opcode: OP_EQ)**
- Format: EQ src1, src2
- Operands: 2 (source1, source2)
- Description: Equality comparison, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int(value_a == value_b)`
- Note: Values are unpacked as integers before comparison

**LT (Opcode: OP_LT)**
- Format: LT src1, src2
- Operands: 2 (source1, source2)
- Description: Less-than comparison, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int(value_a < value_b)`
- Note: Values are unpacked as integers before comparison

**GT (Opcode: OP_GT)**
- Format: GT src1, src2
- Operands: 2 (source1, source2)
- Description: Greater-than comparison, result stored in register 21 (LOGIC_REG)
- Implementation: `registers[21] = pack_int(value_a > value_b)`
- Note: Values are unpacked as integers before comparison

### Control Flow

**JMP (Opcode: OP_JMP)**
- Format: JMP addr
- Operands: 1 (address)
- Description: Unconditional jump to address
- Status: **Not implemented** - case exists in interpreter switch but no implementation

**JZ (Opcode: OP_JZ)**
- Format: JZ value, addr
- Operands: 2 (value to test, jump address)
- Description: Jump to address if value is zero
- Status: **Not implemented** - case exists in interpreter switch but no implementation

**JNZ (Opcode: OP_JNZ)**
- Format: JNZ value, addr
- Operands: 2 (value to test, jump address)
- Description: Jump to address if value is not zero
- Status: **Not implemented** - case exists in interpreter switch but no implementation

**CALL (Opcode: OP_CALL)**
- Format: CALL function_name
- Operands: 1 (heap pointer to function name string)
- Description: Call function by name, creates new stack frame
- Implementation: 
  - Calls `push_stack_scope()` to create new stack frame
  - Retrieves function name from `heap[caller_id_pointer].value.string_value`
  - Looks up function address in `procedure_label_encoding` map
  - Sets `program_pointer` to function start address

**RET (Opcode: OP_RET)**
- Format: RET
- Operands: 0
- Description: Return from current function
- Implementation:
  - Restores `program_pointer` from `frame_pointer->return_address`
  - Calls `pop_stack_scope()` to restore previous stack frame

### Memory Operations

SageVM uses a managed heap system accessed through heap pointers. Memory operations are handled through the interpreter's heap map (`map<int, SageValue> heap`) rather than explicit allocation instructions.

**Heap Storage**
- Function: `store_in_heap(SageValue value)` returns heap pointer
- Implementation: `heap[pointer] = value; return pointer;`
- Used for storing complex data types and function names

### Special Instructions

**NOP (Opcode: OP_NOP)**
- Format: NOP
- Operands: 0
- Description: No operation, does nothing
- Implementation: Execution continues to next instruction

**SYSCALL (Opcode: OP_SYSCALL)**
- Format: SYSCALL
- Operands: 0 (uses register 22 for syscall ID)
- Description: System call using ID from register 22
- Status: **Not implemented** - function exists but contains only TODO comments

**LABEL (Opcode: OP_LABEL)**
- Format: LABEL
- Operands: 0
- Description: Code label marker (does nothing during execution)
- Usage: Used for marking function entry points and jump targets
- Implementation: No-op during execution, handled by switch case fall-through with OP_NOP

## Bytecode File Format

SageVM bytecode is represented as a vector of `command` structures in memory. Each command contains:

```cpp
struct command {
    instruction inst;   // Contains opcode and packed operands
    int deref_map[4];  // Dereference mode for each operand
};
```

### Dereference Modes
- **0**: Raw value (use operand directly, packed as integer)
- **1**: Register dereference (use operand as register index to access `registers[operand]`)
- **2**: Stack dereference (use operand as stack offset) - *Not currently implemented in dereference_map*
- **3**: Heap dereference (use operand as heap pointer) - *Not currently implemented in dereference_map*

## Data Types

SageVM supports three main data types stored in 64-bit registers with type tagging:

**Integer (I32_REG = 0)**
- 32-bit signed integers stored in upper 32 bits
- Type tag: 0 in lower 3 bits
- Packed using `pack_int(int32_t value)` and unpacked using `unpack_int(ui64 reg)`
- Packing formula: `(static_cast<ui64>(value) << 32) | I32_REG`

**Float (F32_REG = 1)**
- 32-bit floating-point numbers stored as bits in upper 32 bits
- Type tag: 1 in lower 3 bits  
- Packed using `pack_float(float value)` and unpacked using `unpack_float(uint64_t reg)`
- Uses union for bit manipulation between float and uint32_t representations

**Pointer (PTR_REG = 2)**
- Memory addresses (heap pointers, void*)
- Type tag: 2 in lower 3 bits
- Packed using `pack_ptr(void* value)` and unpacked using `unpack_pointer(ui64 reg)`
- Packing formula: `reinterpret_cast<uintptr_t>(value) | PTR_REG`

## Error Handling

Error handling in SageVM is currently implemented through direct panic messages printed to stdout. Common error conditions include:

**Division by Zero**
- Triggered when OP_DIV operand 2 is zero
- Message: `"PANIC(SageInterpreter/execute_div) : div cannot divide by 0!"`

**Invalid Operand Count**
- Triggered when instructions receive insufficient operands
- ADD/SUB/MUL/DIV: `"PANIC(SageInterpreter/execute_[op]) : execution requires 3 operands but less were found!"`
- LOAD/STORE/MOV: `"PANIC(SageInterpreter/execute_[op]): execution expects 2 operands but found less!"`

**Stack Overflow**
- Detected when `return_address + 1 == stack.size()` during stack frame push
- Message: `"stackoverflow (todo: make this error better)"`
- Uses `ErrorLogger::get().log_error()` with GENERAL error type

**Unrecognized Bytecode**
- Triggered by unknown opcode in interpreter switch statement
- Uses `ErrorLogger::get().log_internal_error()` for unhandled opcodes

## Instruction Encoding Reference

### Opcode Values
Based on the source code enumeration in `sage_bytecode.h`:

```
OP_ADD = 0          OP_JMP = 7          OP_EQ = 13
OP_SUB = 1          OP_JZ = 8           OP_LT = 14  
OP_MUL = 2          OP_JNZ = 9          OP_GT = 15
OP_DIV = 3          OP_CALL = 10        OP_AND = 16
OP_LOAD = 4         OP_RET = 11         OP_OR = 17
OP_STORE = 5        OP_SYSCALL = 20     OP_NOT = 18
OP_MOV = 6          OP_LABEL = 21       OP_NOP = 19
```

**Note**: The enum uses automatic incrementing, so each subsequent value is incremented by 1.

### Operand Packing

**Two Operands (dpack/dunpack)**
- Used by: MOV, LOAD, STORE, AND, OR, EQ, LT, GT, JZ, JNZ
- Format: 16-bit operand1 in upper 16 bits, 16-bit operand2 in lower 16 bits
- Packing: `(static_cast<ui32>(operand1) << 16) | operand2`
- Unpacking: `{static_cast<ui16>(packed >> 16), static_cast<ui16>(packed & 0xFFFF)}`

**Three Operands (tpack/tunpack)**  
- Used by: ADD, SUB, MUL, DIV
- Format: 8-bit operands packed into 32-bit value 
- Packing: `(op1 << 24) | (op2 << 16) | (op3 << 8) | op4`
- Unpacking: Returns struct with `{op1, op2, op3}` (op4 is available but not used)

**Single Operand**
- Used by: NOT, JMP, CALL, LABEL
- Format: 32-bit operand stored directly
- Used as-is without packing/unpacking

## VM Execution Model

**Program Execution**
- Programs are executed by calling `interpreter.execute()`
- Execution starts at `program_pointer = 0` and continues until end of program
- Each instruction increments `program_pointer` by 1 after execution
- Stack pointer starts at 0 in register 23

**Stack Frame Management**
- Function calls create new stack frames using `push_stack_scope()`
- Returns restore previous frames using `pop_stack_scope()`
- Each frame tracks: return address, stack pointer, previous frame, saved registers
- Stack overflow detected when `return_address + 1 == stack.size()`

**Volatile Register Management**
- Volatile registers (10-20) are tracked in `available_volatiles` set
- `get_volatile_register()` allocates next available volatile
- `register_is_stale()` checks if symbol values match register contents

## Examples

### Simple Addition
```cpp
// Add values from registers 1 and 2, store result in register 0
int deref_map[4] = {0, 1, 1, 0}; // dst=raw index, src1=reg, src2=reg
command add_cmd(OP_ADD, 0, 1, 2, deref_map);
```

### Function Call
```cpp
// Call function whose name is stored at heap pointer 5
int deref_map[4] = {3, 0, 0, 0}; // heap deref for function name
command call_cmd(OP_CALL, 5, deref_map);
```

### Move Immediate Value
```cpp
// Move immediate value 42 into register 10
int deref_map[4] = {0, 0, 0, 0}; // raw value to raw register index
command mov_cmd(OP_MOV, 42, 10, deref_map);
```

## Implementation Status

**Fully Implemented Instructions:**
- Arithmetic: ADD, SUB, MUL, DIV
- Data Movement: MOV, LOAD, STORE
- Logical: AND, OR, NOT
- Comparison: EQ, LT, GT
- Control Flow: CALL, RET
- Special: NOP, LABEL

**Not Implemented:**
- Control Flow: JMP, JZ, JNZ (cases exist but no implementation)
- System: SYSCALL (function exists but empty, contains TODO)

**Key Features:**
- 125-register architecture with typed 64-bit registers
- Stack-based function call system with automatic frame management
- Managed heap with `SageValue` storage
- Type-safe register operations with pack/unpack functions
- Volatile register tracking for optimization

---

*© [2025] [Aleksandar Simpson].*
*SageVM Version [0.0.1]*
