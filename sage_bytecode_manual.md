# SageVM Bytecode Reference Manual

## Table of Contents
- [Introduction](#introduction)
- [Register Set](#register-set)
- [Instruction Format](#instruction-format)
- [Instruction Set](#instruction-set)
  - [Data Movement Instructions](#data-movement-instructions)
  - [Arithmetic Operations](#arithmetic-operations)
  - [Logical Operations](#logical-operations)
  - [Control Flow](#control-flow)
  - [Memory Operations](#memory-operations)
  - [Special Instructions](#special-instructions)
- [Bytecode File Format](#bytecode-file-format)
- [Error Handling](#error-handling)

## Introduction

This document describes the bytecode specification for SageVM, a virtual machine designed to support the Sage interpreter. SageVM uses a register-based architecture with 31 registers and supports various data types including integers, floats, and pointers.

## Register Set

SageVM provides 31 registers (0-30) with the following allocation:

**Function Parameter Registers (0-5)**
- Used for passing function arguments

**Return Value Registers (6-9)**
- Used for function return values

**General Purpose Registers (10-20)**
- Available for general computation

**System Registers (21-23)**
- Register 21 (LOGIC_REG): Stores boolean/logical operation results
- Register 22 (SYSCALL_REG): Used for system call operations
- Register 23 (STACK_POINTER): Current stack frame pointer

**Volatile Registers (24-30)**
- Used for temporary values and intermediate results

## Instruction Format

Instructions consist of:
- **Opcode**: Identifies the operation type
- **Operands**: Up to 4 operands packed into a 32-bit value
- **Address Map**: Defines how operands should be dereferenced (0=raw, 1=register, 2=stack, 3=heap)

## Instruction Set

### Data Movement Instructions

**MOV (Opcode: OP_MOV)**
- Format: MOV src, dst
- Operands: 2 (source, destination)
- Description: Copy value from source operand to destination register
- Implementation: `registers[dst] = src_value`

**LOAD (Opcode: OP_LOAD)**
- Format: LOAD offset, dst
- Operands: 2 (stack offset, destination register)
- Description: Load value from stack at (stack_pointer + offset) into destination register
- Implementation: Loads from `stack[stack_pointer + offset]`

**STORE (Opcode: OP_STORE)**
- Format: STORE src, offset
- Operands: 2 (source register, stack offset)
- Description: Store value from source register to stack at (stack_pointer + offset)
- Implementation: Stores to `stack[stack_pointer + offset]`

### Arithmetic Operations

**ADD (Opcode: OP_ADD)**
- Format: ADD dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer addition: `dst = src1 + src2`
- Implementation: `registers[dst] = pack_int(unpack_int(src1) + unpack_int(src2))`

**SUB (Opcode: OP_SUB)**
- Format: SUB dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer subtraction: `dst = src1 - src2`
- Implementation: `registers[dst] = pack_int(unpack_int(src1) - unpack_int(src2))`

**MUL (Opcode: OP_MUL)**
- Format: MUL dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Integer multiplication: `dst = src1 * src2`
- Implementation: `registers[dst] = pack_int(unpack_int(src1) * unpack_int(src2))`

**DIV (Opcode: OP_DIV)**
- Format: DIV dst, src1, src2
- Operands: 3 (destination register, source1, source2)
- Description: Division: `dst = src1 / src2` (result is stored as float)
- Implementation: `registers[dst] = pack_float(unpack_int(src1) / unpack_int(src2))`
- Note: Division by zero triggers a panic

### Logical Operations

**AND (Opcode: OP_AND)**
- Format: AND src1, src2
- Operands: 2 (source1, source2)
- Description: Logical AND operation, result stored in register 21
- Implementation: `registers[21] = pack_int((src1 && src2) == 1)`

**OR (Opcode: OP_OR)**
- Format: OR src1, src2
- Operands: 2 (source1, source2)
- Description: Logical OR operation, result stored in register 21
- Implementation: `registers[21] = pack_int((src1 || src2) == 1)`

**NOT (Opcode: OP_NOT)**
- Format: NOT src
- Operands: 1 (source)
- Description: Logical NOT operation, result stored in register 21
- Implementation: `registers[21] = pack_int(!src)`

### Comparison Operations

**EQ (Opcode: OP_EQ)**
- Format: EQ src1, src2
- Operands: 2 (source1, source2)
- Description: Equality comparison, result stored in register 21
- Implementation: `registers[21] = pack_int(src1 == src2)`

**LT (Opcode: OP_LT)**
- Format: LT src1, src2
- Operands: 2 (source1, source2)
- Description: Less-than comparison, result stored in register 21
- Implementation: `registers[21] = pack_int(src1 < src2)`

**GT (Opcode: OP_GT)**
- Format: GT src1, src2
- Operands: 2 (source1, source2)
- Description: Greater-than comparison, result stored in register 21
- Implementation: `registers[21] = pack_int(src1 > src2)`

### Control Flow

**JMP (Opcode: OP_JMP)**
- Format: JMP addr
- Operands: 1 (address)
- Description: Unconditional jump to address
- Status: Not yet implemented (TODO)

**JZ (Opcode: OP_JZ)**
- Format: JZ value, addr
- Operands: 2 (value to test, jump address)
- Description: Jump to address if value is zero
- Status: Not yet implemented (TODO)

**JNZ (Opcode: OP_JNZ)**
- Format: JNZ value, addr
- Operands: 2 (value to test, jump address)
- Description: Jump to address if value is not zero
- Status: Not yet implemented (TODO)

**CALL (Opcode: OP_CALL)**
- Format: CALL function_name
- Operands: 1 (heap pointer to function name string)
- Description: Call function by name, creates new stack frame
- Implementation: 
  - Pushes new stack scope
  - Looks up function address in `procedure_label_encoding`
  - Sets program pointer to function start

**RET (Opcode: OP_RET)**
- Format: RET
- Operands: 0
- Description: Return from current function
- Implementation:
  - Restores program pointer from current frame's return address
  - Pops current stack frame

### Memory Operations

**ALLOC (Opcode: OP_ALLOC)**
- Format: ALLOC dst, size
- Operands: 2 (destination register, size)
- Description: Allocate memory of specified size on the virtual heap
- Status: Not yet implemented (TODO)

**FREE (Opcode: OP_FREE)**
- Format: FREE addr
- Operands: 1 (heap address)
- Description: Free allocated memory at the specified heap address
- Status: Not yet implemented (TODO)

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
- Status: Not yet implemented (TODO)

**LABEL (Opcode: OP_LABEL)**
- Format: LABEL
- Operands: 0
- Description: Code label marker (does nothing during execution)
- Usage: Used for marking function entry points and jump targets

### Execution Control Instructions

**BEGIN_EXECUTION (Opcode: OP_BEGIN_EXECUTION)**
- Format: BEGIN_EXECUTION
- Operands: 0
- Description: Signals interpreter to begin executing instructions
- Usage: Compile-time directive to start interpretation

**END_EXECUTION (Opcode: OP_END_EXECUTION)**
- Format: END_EXECUTION
- Operands: 0
- Description: Signals interpreter to pause execution
- Usage: Compile-time directive to stop interpretation

## Bytecode File Format

SageVM bytecode is represented as a vector of `command` structures in memory. Each command contains:

```cpp
struct command {
    instruction inst;  // Contains opcode and packed operands
    int map[4];       // Address mode for each operand
};
```

### Address Modes
- **0**: Raw value (use operand directly)
- **1**: Register dereference (use operand as register index)
- **2**: Stack dereference (use operand as stack offset)
- **3**: Heap dereference (use operand as heap pointer)

## Data Types

SageVM supports three main data types stored in 64-bit registers:

**Integer (I32_REG)**
- 32-bit signed integers
- Packed using `pack_int()` and unpacked using `unpack_int()`

**Float (F32_REG)**
- 32-bit floating-point numbers
- Packed using `pack_float()` and unpacked using `unpack_float()`

**Pointer (PTR_REG)**
- Memory addresses (heap or stack pointers)
- Packed using `pack_ptr()` and unpacked using `unpack_pointer()`

## Error Handling

Error handling in SageVM is currently implemented through direct panic messages. Common error conditions include:

**Division by Zero**
- Triggered when OP_DIV operand 2 is zero
- Prints: "PANIC(SageInterpreter/execute_div) : div cannot divide by 0!"

**Invalid Operand Count**
- Triggered when instructions receive insufficient operands
- Example: "PANIC(SageInterpreter/execute_add) : execution requires 3 operands but less were found!"

**Stack Overflow**
- Detected when return address + 1 equals stack size during stack frame push

## Instruction Encoding Reference

### Opcode Values
Based on the source code enumeration in `sage_bytecode.h`:

```
OP_ADD = 0          OP_JMP = 9          OP_EQ = 14
OP_SUB = 1          OP_JZ = 10          OP_LT = 15  
OP_MUL = 2          OP_JNZ = 11         OP_GT = 16
OP_DIV = 3          OP_CALL = 12        OP_AND = 17
OP_LOAD = 4         OP_RET = 13         OP_OR = 18
OP_STORE = 5        OP_ALLOC = 7        OP_NOT = 19
OP_MOV = 6          OP_FREE = 8         OP_NOP = 20
OP_SYSCALL = 21     OP_LABEL = 22
OP_BEGIN_EXECUTION = 23
OP_END_EXECUTION = 24
```

### Operand Packing

**Two Operands (dpack/dunpack)**
- Used by: MOV, LOAD, STORE
- Format: 16-bit operand1 in upper 16 bits, 16-bit operand2 in lower 16 bits

**Three+ Operands (tpack/tunpack)**  
- Used by: ADD, SUB, MUL, DIV, comparison operations
- Format: 8-bit operands packed into 32-bit value (op1 << 24 | op2 << 16 | op3 << 8 | op4)

## Examples

### Simple Addition
```
// Add registers 1 and 2, store result in register 0
command add_cmd(OP_ADD, 0, 1, 2, {1, 1, 1, 0});
// map[0]=1: register deref for dst
// map[1]=1: register deref for src1  
// map[2]=1: register deref for src2
```

### Function Call
```
// Call function whose name is stored at heap pointer 5
command call_cmd(OP_CALL, 5, {3, 0, 0, 0});
// map[0]=3: heap deref for function name pointer
```

---

*© [2025] [Aleksandar Simpson].*
*SageVM Version [0.0.1]*
