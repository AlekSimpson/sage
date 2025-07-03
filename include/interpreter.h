#pragma once

#include "sage_types.h"
#include "node_manager.h"
#include "codegen.h"
#include "sage_bytecode.h"

#define LOGIC_REG 21
#define SYSCALL_REG 22
#define STACK_POINTER 23

#define offset_t int

class StackFrame {
public:
  int return_address; 
  int stack_pointer;
  StackFrame* previous_frame;
  map<int, int> saved_caller_values;
  map<string, offset_t> local_variables; // tracks variable allocations

  StackFrame(StackFrame* previous, 
	     map<int, int> caller_cache, 
	     int ret_addr, 
	     int stack_pointer);
};

class SageInterpreter {
public:
  // sage argument registers
  // - 0-5  = function parameter registers
  // - 6-9  = return value registers
  // - 10-20 = general registers
  // - 21-23 = system registers
  // - 24-30 = volatile registers (hold results of temp values and stuff)
  //
  // sr21 = bool logical result register
  // sr22 = subroutine return register, holds program pointer
  // sr23 = stack pointer
  //

  int registers[31];
  int program_pointer;
  int procedure_encoding; // starts at 0
  map<string, int> procedure_label_encoding;
  StackFrame* frame_pointer; // keeps track of current frame

  bytecode program;
  map<int, SageValue> heap;
  vector<SageValue> stack;

  SageInterpreter();
  SageInterpreter(int _stack_size);

  void close();

  int store_in_heap(SageValue value);
  void load_program(bytecode program);
  void execute(int begin_program_counter);

  void push_stack_scope(string scopename);
  void pop_stack_scope(); // pops current stack frame
  vector<int> dereference_map(instruction*, int[4]);
};








