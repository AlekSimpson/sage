#pragma once

#include "sage_types.h"
#include "node_manager.h"
#include "sage_bytecode.h"
#include "symbols.h"
#include <set>

#define LOGIC_REG 21
#define SYSCALL_REG 22
#define STACK_POINTER 23

#define offset_t int
#define ui64 uint64_t

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
  StackFrame();
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
  // sr22 = syscall register
  // sr23 = stack pointer
  //

  ui64 registers[31];
  int program_pointer;
  int procedure_encoding; // starts at 0
  map<string, int> procedure_label_encoding;
  StackFrame* frame_pointer; // keeps track of current frame
  set<int> available_volatiles;

  bytecode program;
  map<int, SageValue> heap;
  vector<SageValue> stack;

  SageInterpreter();
  SageInterpreter(int stack_size);

  void close();

  int store_in_heap(SageValue value);
  void load_program(bytecode program);
  bool volatile_is_stale(SageSymbol* symbol, int volatile_register);
  int get_volatile_register();
  void execute();

  void push_stack_scope();
  void pop_stack_scope(); // pops current stack frame
  vector<ui64> dereference_map(instruction*, int[4]);

  void execute_add(vector<ui64>);
  void execute_sub(vector<ui64>);
  void execute_mul(vector<ui64>);
  void execute_div(vector<ui64>);
  void execute_load(vector<ui64>);
  void execute_store(vector<ui64>);
  void execute_mov(vector<ui64>);
  void execute_call(vector<ui64>);
  void execute_return();
  void execute_eqcomp(vector<ui64>);
  void execute_ltcomp(vector<ui64>);
  void execute_gtcomp(vector<ui64>);
  void execute_and(vector<ui64>);
  void execute_or(vector<ui64>);
  void execute_not(vector<ui64>);
  void execute_syscall();
};








