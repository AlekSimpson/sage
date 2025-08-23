#pragma once
#include "sage_bytecode.h"
#include <map>
#include <stack>

struct procedure_frame {
  string name;
  bytecode procedure_instructions;

  procedure_frame() : name("") {}
  procedure_frame(string name) : name(name) {}
};

struct BytecodeBuilder {
  map<int, procedure_frame> procedures;
  stack<int> procedure_stack;
  int blank_encoding[4] = {0, 0, 0, 0};
  int first_encoding[4] = {1, 0, 0, 0};
  int total_instruction_count = 0;

  BytecodeBuilder();

  void add_instruction(SageOpCode, int);
  void add_instruction(SageOpCode, int, int (&map)[4]);
  void add_instruction(SageOpCode, int, int, int (&map)[4]);
  void add_instruction(SageOpCode, int, int, int, int (&map)[4]);
  void add_instruction(SageOpCode, int, int, int, int, int (&map)[4]);

  void new_frame(string name);
  void exit_frame();

  void reset();
  bytecode final();
  int current_id();
};



































