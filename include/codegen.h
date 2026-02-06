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

#define assertm(condition, message) assert((condition) && (message))

#define ui32 uint32_t
#define ui64 uint64_t

#define GENERAL_REGISTER_COUNT 100
#define GENERAL_REG_RANGE_BEGIN 24
#define GENERAL_REG_RANGE_END 124
#define BUILTIN_COUNT 14

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

enum CompilationTarget {
  SAGE_VM,
  X86, // 32 bit legacy support
  X86_64, // modern
  ARM32, // pi / embedded support
  ARM64, // apple silicon
  AARCH64_64, // apple silicon, mobile, AWG Graviton, servers
  RISCV,
  WEBASM
};

string compilation_target_string(CompilationTarget target);

struct CompilerOptions {
  string input_file = "";
  string output_file = "s.out";
  debug_level debug = NONE;
  CompilationTarget compilation_target = SAGE_VM;
  bool emit_bytecode = false;

  CompilerOptions(string input_file): input_file(input_file) {};
};

CompilerOptions parse_compiler_flags(int, char **);
bool check_filename_valid(const string &filename);

enum class VisitorResultState { IMMEDIATE, SPILLED, REGISTER, VALUE }; // , LIST

struct VisitorResult {
  table_index symbol_table_index = SAGE_NULL_SYMBOL;
  SageValue immediate_value = SageValue();
  vector<VisitorResult> list_results;

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
  CompilerOptions options;

  NodeManager *node_manager;
  ScopeManager scope_manager;
  ErrorLogger &logger = ErrorLogger::get();
  SageParser parser;
  SageSymbolTable symbol_table;
  BytecodeBuilder builder;
  ComptimeManager comptime_manager;

  set<string> previously_processed; // for forward decl auto resolution
  map<string, set<string>> definition_dependencies;
  map<string, int> in_degree_map; // TODO: rename, this is not a very good name
  map<table_index, vector<uint8_t>> static_program_memory;
  vector<table_index> static_program_memory_insertion_order;

  CodegenMode codegen_mode;
  const int VOLATILE_REGISTER_SIZE = 10;
  const int VOLATILE_FLOAT_REGISTER_SIZE = 29;
  int volatile_index = 0;
  int volatile_float_index = 0;

  SageCompiler(CompilerOptions options);
  ~SageCompiler();

  void compile_file(string mainfile);

  bool generating_compile_time_bytecode();
  void register_allocation();
  int get_volatile_register();
  int get_volatile_float_register();
  void scan_all_program_symbols(NodeIndex root);
  void perform_type_resolution();
  void forward_declaration_resolution(int program_root);
  void get_in_degree_of(const string &root_definition_identifier, NodeIndex current_node, int working_sope);
  void resolve_definition_order(int target_scope);
  void process_escape_sequences(string &str);
  bool is_float_operation(VisitorResult &one, VisitorResult &two);
  int get_literal_static_pointer(table_index literal_symbol_table_index);

  /* builders */
  VisitorResult build_store(VisitorResult rhs, symbol_entry *var_symbol);
  VisitorResult build_function_with_block(string);
  VisitorResult build_alloca(symbol_entry *var_symbol);
  VisitorResult build_add(VisitorResult, VisitorResult);
  VisitorResult build_sub(VisitorResult, VisitorResult);
  VisitorResult build_div(VisitorResult, VisitorResult);
  VisitorResult build_mul(VisitorResult, VisitorResult);
  VisitorResult build_and(VisitorResult, VisitorResult);
  VisitorResult build_or(VisitorResult, VisitorResult);
  VisitorResult build_load(NodeIndex);
  VisitorResult build_operator(VisitorResult, VisitorResult, SageOpCode, bool is_float_operation);
  int materialize_to_float_register(VisitorResult &);

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
