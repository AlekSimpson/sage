#pragma once

#include "sage_types.h"
#include "node_manager.h"
#include "codegen.h"
#include "sage_bytecode.h"

#define LOGIC_REG 21
#define SYSCALL_REG 22
#define PROGRAM_POINTER 23
#define STACK_POINTER 24

#define offset_t int

class StackFrame {
public:
  int stack_height;
  stack<string> current_scope_stack;
  unordered_map<string, offset_t> allocations; // tracks variable allocations

  // tracks where the scopes of certain functions begin
  // used to know how much to collapse the stack by when a function returns
  unordered_map<string, int> function_scopes;

  StackFrame();
  
  void add_allocation(string name); // increments the stack height
  offset_t get_allocation_offset(string name);
  void push_stack_scope(string scopename);
  void pop_stack_scope();
};

class SageInterpreter {
public:
  // sage argument registers
  // - 0-5  = function parameter registers
  // - 6-9  = return value registers
  // - 10-20 = general registers
  // - 21-24 = system registers
  // - 25-30 = volatile registers (hold results of temp values and stuff)
  //
  // sr21 = bool logical result register
  // sr22 = syscall register
  // sr23 = subroutine return register, holds program pointer
  // sr24 = stack pointer
  //

  int program_counter;
  int stack_size;
  StackFrame stack_frame;
  int registers[31];

  bytecode program;
  vector<SageValue&> heap;
  SageValue&* stack;

  SageInterpreter();
  SageInterpreter(int _stack_size);

  string get_string(SageArrayValue);
  void load_program(vector<instruction> program);
  SageValue* run_program();
};








