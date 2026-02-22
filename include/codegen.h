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
#include "sage_value.h"

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
    bool debug_print_bytecode = false;

    CompilerOptions(string input_file): input_file(input_file) {
    };
};

CompilerOptions parse_compiler_flags(int, char **);

bool check_filename_valid(const string &filename);

class SageCompiler;

enum class VisitorResultState { IMMEDIATE, SPILLED, REGISTER, VALUE, TEMP_REGISTER, LIST };

using TR = TypeRegistery;

struct VisitorResult {
    SageType *result_type;
    SageValue immediate_value = SageValue();
    vector<VisitorResult> list_results;
    int temporary_result_register = -1;
    SymbolIndex symbol_table_index = SAGE_NULL_SYMBOL;
    VisitorResultState state = VisitorResultState::VALUE;

    VisitorResult(): result_type(TR::get_byte_type(VOID)) {
    };

    VisitorResult(int value, SageType *type, SymbolIndex index, bool is_temporary_register = false): symbol_table_index(
        index) {
        if (is_temporary_register) {
            temporary_result_register = value;
            state = VisitorResultState::TEMP_REGISTER;
        } else {
            immediate_value = SageValue(value);
            state = VisitorResultState::IMMEDIATE;
        }
        result_type = type;
    }

    VisitorResult(int value, SageType *type, bool is_temporary_register = false) {
        if (is_temporary_register) {
            temporary_result_register = value;
            state = VisitorResultState::TEMP_REGISTER;
        } else {
            immediate_value = SageValue(value);
            state = VisitorResultState::IMMEDIATE;
        }
        result_type = type;
    }

    VisitorResult(SageValue &value) : state(VisitorResultState::IMMEDIATE) {
        immediate_value = value;
        result_type = immediate_value.type;
    }

    VisitorResult(SageSymbolTable &table, SymbolIndex symbol_index) : symbol_table_index(symbol_index) {
        auto search = table.lookup_by_index(symbol_index);
        if (search->spilled) {
            state = VisitorResultState::SPILLED;
        } else if (table.lookup_by_index(symbol_index)->assigned_register != -1) {
            state = VisitorResultState::REGISTER;
        } else {
            state = VisitorResultState::VALUE;
        }
        result_type = search->datatype;
    }

    void to_register_instruction(SageCompiler &compiler, int argument_register, SageType *argument_type);

    void to_stack_instruction(SageCompiler &compiler, int offset, AddressMode offset_mode = _00);

    pair<int64_t, bool> materialize_register(SageCompiler &compiler);

    bool is_temporary() { return temporary_result_register != -1; }
    bool is_immediate_float() { return !immediate_value.is_null() && immediate_value.type->identify() == FLOAT; }
    bool is_immediate_int() { return !immediate_value.is_null() && immediate_value.type->identify() == INT; }
    bool is_null() { return symbol_table_index == SAGE_NULL_SYMBOL; }
};

struct FieldAccessTreeIterator {
    vector<NodeIndex> elements;
    int index = 0;

    FieldAccessTreeIterator(NodeIndex root, NodeManager *nm) {
        NodeIndex current_root = root;
        while (nm->get_host_nodetype(current_root) == PN_BINARY) {
            elements.push_back(nm->get_left(current_root));
            current_root = nm->get_right(current_root);
        }
        elements.push_back(current_root);
    }

    NodeIndex next();

    bool has_next();
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

    // for forward decl auto resolution
    set<string> previously_processed;
    map<string, set<string> > definition_dependencies;
    map<string, int> in_degree_map; // TODO: rename, this is not a very good name

    ByteVector static_program_memory_store;

    CodegenMode codegen_mode;
    const int VOLATILE_REGISTER_SIZE = 10;
    int volatile_index = 0;

    SageCompiler(CompilerOptions options);

    ~SageCompiler();

    void compile_file(string mainfile);

    bool generating_compile_time_bytecode();

    void register_allocation();

    int get_volatile_register();

    void scan_all_program_symbols(NodeIndex root);

    void perform_type_resolution();

    void forward_declaration_resolution(int program_root);

    void get_in_degree_of(const string &root_definition_identifier, NodeIndex current_node, int working_sope);

    void resolve_definition_order(int target_scope);

    void process_escape_sequences(string &str);

    bool is_float_operation(VisitorResult &one, VisitorResult &two);

    int get_literal_static_pointer(SymbolIndex literal_symbol_table_index);

    /* builders */
    VisitorResult build_store(VisitorResult rhs, SymbolEntry *var_symbol);

    VisitorResult build_function_with_block(string);

    VisitorResult build_alloca(SymbolEntry *var_symbol);

    VisitorResult build_add(VisitorResult, VisitorResult);

    VisitorResult build_sub(VisitorResult, VisitorResult);

    VisitorResult build_div(VisitorResult, VisitorResult);

    VisitorResult build_mul(VisitorResult, VisitorResult);

    VisitorResult build_and(VisitorResult, VisitorResult);

    VisitorResult build_or(VisitorResult, VisitorResult);

    VisitorResult build_operator(VisitorResult, VisitorResult, SageOpCode);

    VisitorResult build_dereference_instructions(VisitorResult &, Token);

    /* visitors */
    VisitorResult visit(NodeIndex);

    VisitorResult visit_struct_field_access(NodeIndex, bool for_assignment = false);

    VisitorResult visit_statement(NodeIndex);

    VisitorResult visit_keyword(NodeIndex);

    VisitorResult visit_function_definition(NodeIndex);

    VisitorResult visit_if(NodeIndex);

    VisitorResult visit_while(NodeIndex);

    VisitorResult visit_for(NodeIndex);

    VisitorResult visit_variable_definition(NodeIndex);

    VisitorResult visit_variable_assign(NodeIndex);

    VisitorResult visit_function_return(NodeIndex);

    VisitorResult visit_expression(NodeIndex);

    VisitorResult visit_literal(NodeIndex);

    VisitorResult visit_function_call(NodeIndex, int first_parameter_pointer_register = -1);

    VisitorResult visit_binary_operator(NodeIndex);
};
