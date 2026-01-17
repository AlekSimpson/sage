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
#define BUILTIN_COUNT 14

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

  NodeManager *node_manager;
  ScopeManager scope_manager;
  ErrorLogger &logger = ErrorLogger::get();
  SageParser parser;
  SageSymbolTable symbol_table;
  SageInterpreter *interpreter;
  BytecodeBuilder builder;
  set<NodeIndex> precompiled;
  ascending_list<comptime_ast_bookmark> bookmarked_run_directives;
  bool interpreter_mode = false;
  map<int, SageValue> volatile_register_state;
  int volatile_index = 0;

  set<string> previously_processed; // for forward decl auto resolution
  map<string, set<string>> definition_dependencies; // the in degree is just the
  map<string, int> in_degree_map;

  SageCompiler();
  SageCompiler(string mainfile);
  ~SageCompiler();

  bool check_filename_valid(string filename);
  NodeIndex parse_codefile(string target_file);
  void begin_compilation(string mainfile);
  bytecode compile(NodeIndex ast);

  void register_allocation();
  bool node_is_precompiled(NodeIndex);
  int get_volatile();
  bool volatile_is_stale(SageValue&, int);
  void print_bytecode(bytecode&);
  void perform_first_compilation_pass(NodeIndex root);
  void forward_declaration_resolution(int program_root);
  void get_in_degree_of(const string &root_definition_identifier, NodeIndex current_node, int working_sope);
  void resolve_definition_order(int target_scope);
  void process_escape_sequences(string &str);

  /* builders */
  ui32 build_store(ui32 rhs, symbol_entry *var_symbol);
  ui32 build_return(ui32, bool);
  ui32 build_function_with_block(string);
  ui32 build_alloca(symbol_entry *var_symbol);
  ui32 build_add(ui32, ui32);
  ui32 build_sub(ui32, ui32);
  ui32 build_div(ui32, ui32);
  ui32 build_mul(ui32, ui32);
  ui32 build_and(ui32, ui32);
  ui32 build_or(ui32, ui32);
  ui32 build_load(NodeIndex);
  ui32 build_function_call(vector<ui32>, int);
  ui32 build_operator(ui32, ui32, SageOpCode);

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
  //ui32 visit_varref(NodeIndex);
  ui32 visit_literal(NodeIndex);
  ui32 visit_funccall(NodeIndex);
  ui32 visit_binop(NodeIndex);
  ui32 visit_unop(NodeIndex);

};
