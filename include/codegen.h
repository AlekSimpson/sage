#pragma once

#include <map>
#include "error_logger.h"
#include "parser.h"
#include "interpreter.h"
#include "node_manager.h"
#include "symbols.h"
#include "bytecode_builder.h"
#include "comptime_manager.h"
#include "sage_bytecode.h"

#define ui32 uint32_t
#define ui64 uint64_t

#define GENERAL_REGISTER_COUNT 100
#define GENERAL_REG_RANGE_BEGIN 24
#define GENERAL_REG_RANGE_END 124
#define BUILTIN_COUNT 13

#define SAGESYS_write_int 600

enum debug_level {
  NONE,
  PARSING,
  COMPILATION,
  ALL
};

enum CodegenMode {
  GEN_RUNTIME,
  GEN_COMPTIME
};

enum class VisitorResultState { IMMEDIATE, SPILLED, REGISTER, VALUE};

struct VisitorResult {
  table_index symbol_table_index = SAGE_NULL_SYMBOL;
  SageValue immediate_value = SageValue();

  VisitorResult() {};
  VisitorResult(int value) : immediate_value(SageValue(value)) {}
  VisitorResult(float value) : immediate_value(SageValue(value)) {}
  VisitorResult(table_index symbol_index) : symbol_table_index(symbol_index) {}

  bool is_immediate() { return !immediate_value.is_null(); }
  bool is_null() { return symbol_table_index == SAGE_NULL_SYMBOL; }
  VisitorResultState get_result_state(SageSymbolTable *);
};

// TODO: create robust debug settings for debugging the compiler
class SageCompiler {
public:
  debug_level debug;

  NodeManager *node_manager;
  ScopeManager scope_manager;
  ErrorLogger &logger = ErrorLogger::get();
  SageParser parser;
  SageSymbolTable symbol_table;
  BytecodeBuilder builder;
  ComptimeManager comptime_manager;

  CodegenMode codegen_mode;
  map<int, SageValue> volatile_register_state;
  int volatile_index = 0;

  set<string> previously_processed; // for forward decl auto resolution
  map<string, set<string>> definition_dependencies;
  map<string, int> in_degree_map;

  SageCompiler();
  ~SageCompiler();

  bool check_filename_valid(const string &filename);
  void compile_file(string mainfile);

  bool generating_compile_time_bytecode();
  void register_allocation();
  int get_volatile();
  bool volatile_is_stale(SageValue&, int);
  void print_bytecode(bytecode&);
  void scan_all_program_symbols(NodeIndex root);
  void perform_type_resolution();
  void forward_declaration_resolution(int program_root);
  void get_in_degree_of(const string &root_definition_identifier, NodeIndex current_node, int working_sope);
  void resolve_definition_order(int target_scope);
  void process_escape_sequences(string &str);

  /* builders -- TODO: these can just be inlined into the visitors, most of these don't need to be their own functions */
  VisitorResult build_store(VisitorResult rhs, symbol_entry *var_symbol);
  VisitorResult build_return(VisitorResult, bool);
  VisitorResult build_function_with_block(string);
  VisitorResult build_alloca(symbol_entry *var_symbol);
  VisitorResult build_add(VisitorResult, VisitorResult);
  VisitorResult build_sub(VisitorResult, VisitorResult);
  VisitorResult build_div(VisitorResult, VisitorResult);
  VisitorResult build_mul(VisitorResult, VisitorResult);
  VisitorResult build_and(VisitorResult, VisitorResult);
  VisitorResult build_or(VisitorResult, VisitorResult);
  VisitorResult build_load(NodeIndex);
  VisitorResult build_operator(VisitorResult, VisitorResult, SageOpCode);

  /* visitors */
  VisitorResult visit(NodeIndex);

  VisitorResult visit_statement(NodeIndex);
  VisitorResult visit_keyword(NodeIndex);
  VisitorResult visit_function_definition(NodeIndex);
  VisitorResult visit_struct(NodeIndex);
  VisitorResult visit_if(NodeIndex);
  VisitorResult visit_while(NodeIndex);
  VisitorResult visit_for(NodeIndex);
  VisitorResult visit_variable_definition(NodeIndex);
  VisitorResult visit_variable_assign(NodeIndex);
  VisitorResult visit_function_return(NodeIndex);

  VisitorResult visit_expression(NodeIndex);
  VisitorResult visit_literal(NodeIndex);
  VisitorResult visit_function_call(NodeIndex);
  VisitorResult visit_binary_operator(NodeIndex);
  VisitorResult visit_unary_operator(NodeIndex);

};
