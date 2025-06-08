#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <map>
#include <uuid/uuid.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "parser.h"
#include "interpreter.h"
#include "node_manager.h"
#include "symbols.h"
#include "analyzer.h"

using namespace llvm;

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

  uuid_t NULL_ID;

  stack<int> current_procedure;
  vector<bytecode> procedures; // global space is the first element in this arrya

  SageCodeGenVisitor();
  SageCodeGenVisitor(NodeManager*, SageInterpreter*, SageAnalyzer*);

  // llvm::Module* get_module();
  uuid_t process_expression(NodeIndex);

  uuid_t build_store(uuid_t rhs, string variable_symbol);
  uuid_t build_return(uuid_t);
  uuid_t build_function_with_block(vector<string>, string);
  uuid_t build_alloca(SageType, string);
  uuid_t build_add(uuid_t, uuid_t);
  uuid_t build_sub(uuid_t, uuid_t);
  uuid_t build_div(uuid_t, uuid_t);
  uuid_t build_mul(uuid_t, uuid_t);
  uuid_t build_and(uuid_t, uuid_t);
  uuid_t build_or(uuid_t, uuid_t);
  uuid_t build_load(SageType, string);
  uuid_t build_constant_int(string);
  uuid_t build_constant_float(string);
  uuid_t build_string_pointer(string);
  uuid_t build_function_call(vector<string>, string)

  uuid_t visit_function_return(uuid_t);
  uuid_t visit_program(NodeIndex);
  uuid_t visit_variable_decl(NodeIndex);
  uuid_t visit_function_declaration(NodeIndex);
  uuid_t visit_function_definition(NodeIndex);
  uuid_t visit_function_call(NodeIndex);
  uuid_t visit_codeblock(NodeIndex);
  uuid_t visit_trinary_expr(NodeIndex);
  uuid_t visit_binary_expr(NodeIndex);
  uuid_t visit_expression(NodeIndex);
  uuid_t visit_variable_assignment(NodeIndex);
  uuid_t visit_unary_expr(NodeIndex);
};

class SageCompiler {
public:
  NodeIndex ast;
  debug_level debug;

  NodeManager* node_manager;
  SageParser parser;
  SageInterpreter* interpreter;
  SageCodeGenVisitor visitor;
  SageAnalyzer* analyzer;

  SageCompiler();
  SageCompiler(string mainfile);
  ~SageCompiler();

  bool check_filename_valid(string filename);
  NodeIndex parse_codefile(string target_file);

  void begin_compilation(string mainfile);
  llvm::Module* compile(NodeIndex ast);
  successful emit_and_link_llvm(llvm::Module* module, const std::string& output_file);
};
