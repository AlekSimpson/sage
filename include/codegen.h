#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <map>
// #include <llvm/IR/LLVMContext.h>
// #include <llvm/IR/Module.h>
// #include <llvm/IR/IRBuilder.h>

#include "parser.h"
#include "interpreter.h"
#include "node_manager.h"
#include "symbols.h"
#include "analyzer.h"
#include "sage_bytecode.h"

#define ui32 uint32_t

enum debug_level {
  NONE,
  LEXING, // TODO: still no way to get lexer debug output
  PARSING,
  COMPILATION,
  ALL
};

class SageCodeGenVisitor {
public:
  SageSymbolTable symbol_table;
  NodeManager* node_manager;
  SageInterpreter* vm;
  SageAnalyzer* analysis;

  stack<int> current_procedure;
  vector<bytecode> procedures; // global space is the first element in this array
  bytecode total_bytecode;

  SageCodeGenVisitor();
  SageCodeGenVisitor(NodeManager*, SageInterpreter*, SageAnalyzer*);

  void add_instruction(SageOpCode, int);
  void add_instruction(SageOpCode, int, int[4]);
  void add_instruction(SageOpCode, int, int, int[4]);
  void add_instruction(SageOpCode, int, int, int, int[4]);
  void add_instruction(SageOpCode, int, int, int, int, int[4]);
  ui32 process_expression(NodeIndex);

  ui32 build_begin();
  ui32 build_end();
  ui32 build_store(ui32 rhs, string variable_symbol);
  ui32 build_return(ui32);
  ui32 build_function_with_block(vector<string>, string);
  ui32 build_alloca(SageType*, string);
  ui32 build_add(ui32, ui32);
  ui32 build_sub(ui32, ui32);
  ui32 build_div(ui32, ui32);
  ui32 build_mul(ui32, ui32);
  ui32 build_and(ui32, ui32);
  ui32 build_or(ui32, ui32);
  ui32 build_load(SageType*, string);
  ui32 build_constant_int(int);
  ui32 build_constant_float(float);
  ui32 build_string_pointer(string);
  ui32 build_function_call(vector<ui32>, string);
  ui32 build_operator(ui32, ui32, SageOpCode);

  ui32 visit_function_return(ui32);
  ui32 visit_program(NodeIndex);
  ui32 visit_variable_decl(NodeIndex);
  ui32 visit_function_declaration(NodeIndex);
  ui32 visit_function_definition(NodeIndex);
  ui32 visit_function_call(NodeIndex);
  ui32 visit_codeblock(NodeIndex);
  ui32 visit_trinary_expr(NodeIndex);
  ui32 visit_binary_expr(NodeIndex);
  ui32 visit_expression(NodeIndex);
  ui32 visit_variable_assignment(NodeIndex);
  ui32 visit_unary_expr(NodeIndex);
};

class SageCompiler {
public:
  NodeIndex ast;
  debug_level debug;

  NodeManager* node_manager;
  SageParser parser;
  SageInterpreter* interpreter;
  SageAnalyzer* analyzer;
  SageCodeGenVisitor visitor;

  SageCompiler();
  SageCompiler(string mainfile);
  ~SageCompiler();

  bool check_filename_valid(string filename);
  NodeIndex parse_codefile(string target_file);

  void begin_compilation(string mainfile);
  bool compile(NodeIndex ast);
  /*successful emit_and_link_llvm(llvm::Module* module, const std::string& output_file);*/
};
