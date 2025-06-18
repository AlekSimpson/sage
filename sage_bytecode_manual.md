# CustomVM Bytecode Reference Manual

## Table of Contents
- [Introduction](#introduction)
- [Register Set](#register-set)
- [Instruction Set](#instruction-set)
  - [Data Movement Instructions](#data-movement-instructions)
  - [Arithmetic Operations](#arithmetic-operations)
  - [Logical Operations](#logical-operations)
  - [Control Flow](#control-flow)
  - [Stack Operations](#stack-operations)
  - [Memory Operations](#memory-operations)
  - [Special Instructions](#special-instructions)
- [Bytecode File Format](#bytecode-file-format)
- [Error Handling](#error-handling)

## Introduction

This document describes the bytecode specification for SageVM, a virtual machine designed to support the sage interpreter. This manual serves as a comprehensive reference for developers creating bytecode for the VM, either directly or through higher-level language compilation.

## Register Set

SageVM provides the following registers: TODO

4. **Additional addressing modes...**

## Instruction Set

### Data Movement Instructions

MOV
- Format: MOV src, dst
- Description: Copy src to dst

LOAD
- Format: LOAD [addr], dst
- Description: Load value from stack memory into dst

STORE
- Format: STORE src, [addr]
- Description: Store src value to stack memory

...

### Arithmetic Operations

ADDR
- Format: ADD dst, src1, src2
- Description: dst = src1 + src2

ADDS
- Format: ADD [dst], src1, src2
- Description: dst = src1 + src2

SUBR
- Format: SUB dst, src1, src2
- Description: dst = src1 - src2

SUBS
- Format: SUB [dst], src1, src2
- Description: dst = src1 - src2

MULR
- Format: MUL dst, src1, src2
- Description: dst = src1 * src2

MULS
- Format: MUL [dst], src1, src2
- Description: dst = src1 * src2

DIV
- Format: DIV dst, src1, src2
- Description: dst = src1 / src2
- if the result is a float then its stored on the stack and a pointer to it on the stack is put in dst register

...

### Logical Operations

AND
- Format: AND src1, src2
- Description: sr21 = src1 & src2

OR
- Format: OR src1, src2
- Description: sr21 = src1 | src2

NOT 
- Format: NOT src
- Description: inverts boolean values, result goes to sr21

GT
- Format: GT src1, src2
- Description: copies truth value into dst if src1 is greater than src2, result goes to sr21

LT
- Format: LT src1, src2
- Description: copies truth value into dst if src1 is less than src2, result goes to sr21

EQ
- Format: EQ src1, src2
- Description: copies truth value into dst if src1 is equal to src2, result goes to sr21

...

### Control Flow

JMP
- Format: JMP addr
- Description: Unconditional jump to addr

JZ
- Format: JZ value, addr
- Description: Jump to addr if value is 0

JNZ
- Format: JNZ value, addr
- Description: Jump to addr if value is not 0

CALL
- Format: CALL label
- Description: Call subroutine at label, automatically moves the call program pointer into sr23

RET
- Format: RET
- Description: Return from subroutine back to program location set in sr23

...

...

### Memory Operations

ALLOC
- Format: ALLOC dst, size
- Description: Allocate memory of size bytes on the virtual heap

FREE
- Format: FREE addr
- Description: Free allocated memory at addr on the virtual heap

...

### Special Instructions

<!-- NOP (Opcode: 0x00) -->
<!-- - Format: NOP -->
<!-- - Description: No operation -->
<!-- - Flags Affected: None -->
<!---->
<!-- SYSCALL (Opcode: 0xFF) -->
<!-- - Format: SYSCALL id -->
<!-- - Description: System call with identifier id -->
<!-- - Flags Affected: Depends on syscall -->

...

## Bytecode File Format

SageVM bytecode files use the extension `.svm`.


## Error Handling

When an error occurs during execution, the VM sets the appropriate error code in the status register and triggers an exception. Exception handlers can be registered to handle specific error conditions.

Common error codes:

INVALID_INSTRUCTION (Code: 0x01)
- Description: Invalid opcode encountered

MEMORY_ACCESS_VIOLATION (Code: 0x02)
- Description: Attempted to access invalid memory

DIVISION_BY_ZERO (Code: 0x03)
- Description: Division by zero attempted

...

## Standard Library


## Examples
---

*© [2025] [Aleksandar Simpson].*
*SageVM Version [0.0.1]*
