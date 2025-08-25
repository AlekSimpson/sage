#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <map>
// #include <llvm/IR/LLVMContext.h>
// #include <llvm/IR/Module.h>
// #include <llvm/IR/IRBuilder.h>

#include "error_logger.h"
#include "parser.h"
#include "interpreter.h"
#include "node_manager.h"
#include "symbols.h"
#include "bytecode_builder.h"
#include "sage_bytecode.h"
#include "depgraph.h"
#include "ascending_list.h"

#define ui32 uint32_t
#define ui64 uint64_t

#define GENERAL_REGISTER_COUNT 100
#define GENERAL_REG_RANGE_BEGIN 24
#define GENERAL_REG_RANGE_END 124

enum debug_level {
  NONE,
  PARSING,
  COMPILATION,
  ALL
};

struct comptime_ast_bookmark {
  DependencyGraph* graph;
  NodeIndex ast_position;
  int scope_level;
};

int ast_bookmark_sorter(comptime_ast_bookmark mark);

class SageCompiler {
public:
  NodeIndex ast;
  debug_level debug;

  NodeManager* node_manager;
  ErrorLogger& logger = ErrorLogger::get();
  SageParser parser;
  SageInterpreter* interpreter;
  SageSymbolTable symbol_table;
  BytecodeBuilder builder;
  set<NodeIndex> precompiled;
  ascending_list<comptime_ast_bookmark> bookmarked_run_directives;

  SageCompiler();
  SageCompiler(string mainfile);
  ~SageCompiler();

  bool check_filename_valid(string filename);
  NodeIndex parse_codefile(string target_file);

  void begin_compilation(string mainfile);
  bytecode compile(NodeIndex ast, DependencyGraph* graph);
  /*bool emit_and_link_llvm(llvm::Module* module, const std::string& output_file);*/

  void compile_exec_order(DependencyGraph*);
  void get_expression_identifiers(vector<string>& identifiers, NodeIndex root);
  DependencyGraph* generate_ident_dependencies(NodeIndex cursor, string, int, set<string>*);
  void register_allocation(DependencyGraph*);
  bool node_is_precompiled(NodeIndex);
  ui32 process_expression(NodeIndex);

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

  ui32 visit(NodeIndex, DependencyGraph*);
  ui32 visit_function_return(ui32);
  ui32 visit_variable_decl(NodeIndex);
  ui32 visit_function_declaration(NodeIndex);
  ui32 visit_function_definition(NodeIndex);
  ui32 visit_function_call(NodeIndex);
  ui32 visit_codeblock(NodeIndex, DependencyGraph* depgraph = nullptr);
  ui32 visit_trinary_expr(NodeIndex, DependencyGraph* depgraph = nullptr);
  ui32 visit_binary_expr(NodeIndex, DependencyGraph* depgraph = nullptr);
  ui32 visit_expression(NodeIndex);
  ui32 visit_variable_assignment(NodeIndex);
  ui32 visit_unary_expr(NodeIndex, DependencyGraph* depgraph = nullptr);
};
