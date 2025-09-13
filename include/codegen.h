#pragma once

#include <map>
#include "error_logger.h"
#include "parser.h"
#include "interpreter.h"
#include "node_manager.h"
#include "symbols.h"
#include "bytecode_builder.h"
#include "sage_bytecode.h"
#include "ascending_list.h"

#define ui32 uint32_t
#define ui64 uint64_t

#define GENERAL_REGISTER_COUNT 100
#define GENERAL_REG_RANGE_BEGIN 24
#define GENERAL_REG_RANGE_END 124

#define SAGESYS_write_int 600

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
  SageSymbolTable symbol_table;
  SageInterpreter* interpreter;
  BytecodeBuilder builder;
  set<NodeIndex> precompiled;
  ascending_list<comptime_ast_bookmark> bookmarked_run_directives;
  bool doing_dependency_resolution_order = false;
  bool interpreter_mode = false;
  map<int, SageValue> volatile_register_state;
  int volatile_index = 0;

  SageCompiler();
  SageCompiler(string mainfile);
  ~SageCompiler();

  /* general compiler utilities */
  bool check_filename_valid(string filename);
  NodeIndex parse_codefile(string target_file);
  void begin_compilation(string mainfile);
  bytecode compile(NodeIndex ast);
  void compile_dependency_resolution_order(DependencyGraph*);
  void get_expression_identifiers(vector<NodeIndex>& identifiers, NodeIndex root);
  DependencyGraph* generate_ident_dependencies(NodeIndex cursor, string, int, set<string>*);
  void register_allocation(DependencyGraph*);
  bool node_is_precompiled(NodeIndex);
  ui32 process_expression(NodeIndex);
  int get_volatile();
  bool volatile_is_stale(SageValue&, int);
  void add_mov(int op1, int op2, int (&map)[4]);
  void create_puts();
  void create_puti();
  void print_bytecode(bytecode&);

  /* builders */
  ui32 build_store(ui32 rhs, string variable_symbol);
  ui32 build_return(ui32, bool);
  ui32 build_function_with_block(string);
  ui32 build_alloca(string);
  ui32 build_add(ui32, ui32);
  ui32 build_sub(ui32, ui32);
  ui32 build_div(ui32, ui32);
  ui32 build_mul(ui32, ui32);
  ui32 build_and(ui32, ui32);
  ui32 build_or(ui32, ui32);
  ui32 build_load(string);
  ui32 build_function_call(vector<ui32>, string);
  ui32 build_operator(ui32, ui32, SageOpCode);
  ui32 build_constant_int(string);
  ui32 build_constant_float(string);
  ui32 build_string_pointer(string);

  /* visitors */
  ui32 visit(NodeIndex); // equivalent to visit block

  ui32 visit_statement(NodeIndex);
  ui32 visit_keyword(NodeIndex);
  ui32 visit_funcdef(NodeIndex);
  ui32 visit_struct(NodeIndex);
  ui32 visit_if(NodeIndex);
  ui32 visit_while(NodeIndex);
  ui32 visit_for(NodeIndex);
  ui32 visit_vardec(NodeIndex);
  ui32 visit_varassign(NodeIndex);
  ui32 visit_funcret(NodeIndex);

  ui32 visit_expression(NodeIndex);
  ui32 visit_varref(NodeIndex);
  ui32 visit_literal(NodeIndex);
  ui32 visit_funccall(NodeIndex);
  ui32 visit_binop(NodeIndex);
  ui32 visit_unop(NodeIndex);

};
