#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>

#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_bytecode.h"
#include "../include/bytecode_builder.h"

std::unordered_map<std::pair<CanonicalType, int>, std::unique_ptr<SageType> > TypeRegistery::builtin_types;
std::unordered_map<SageType *, std::unique_ptr<SageType> > TypeRegistery::pointer_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::array_types;
std::unordered_map<std::pair<std::vector<SageType *>, std::vector<SageType *> >, std::unique_ptr<SageType> >
TypeRegistery::function_types;
std::unordered_map<std::string, std::unique_ptr<SageType>> TypeRegistery::struct_types;

int create_procedure_frame_id(const std::string &str) {
    uint64_t hash = 5381;
    for (char c: str) {
        hash = ((hash << 5) + hash) + c;
    }
    return static_cast<int>(hash);
}

BytecodeBuilder::BytecodeBuilder() {
    int global_id = get_procedure_frame_id("GLOBAL");
    comptime_procedures[global_id] = ProcedureFrame("GLOBAL");
    comptime_procedure_stack.push(global_id);

    runtime_procedures[global_id] = ProcedureFrame("GLOBAL");
    runtime_procedure_stack.push(global_id);

    build_puts();
    build_puti();
}


map<int, ProcedureFrame> &BytecodeBuilder::get_active_procedures() {
    if (emitting_comptime) return comptime_procedures;
    return runtime_procedures;
}

stack<int> &BytecodeBuilder::get_active_procedure_stack() {
    if (emitting_comptime) return comptime_procedure_stack;
    return runtime_procedure_stack;
}

int &BytecodeBuilder::get_total_instruction_count() {
    if (emitting_comptime) return comptime_total_instructions;
    return runtime_total_instructions;
}

void BytecodeBuilder::increment_total_instruction_count(int delta) {
    if (emitting_comptime) {
        comptime_total_instructions += delta;
        return;
    }
    runtime_total_instructions += delta;
}

void BytecodeBuilder::enter_comptime() {
    emitting_comptime = true;
}

void BytecodeBuilder::exit_comptime() {
    emitting_comptime = false;
}

void BytecodeBuilder::new_frame(string name) {
    auto frame = ProcedureFrame(name);
    if (!emitting_comptime && name == "main") {
        runtime_has_main_function = true;
    }
    int id = create_procedure_frame_id(name);

    auto &active_procedure_stack = get_active_procedure_stack();
    auto &active_procedures = get_active_procedures();
    active_procedures[id] = frame;
    active_procedure_stack.push(id);
}

void BytecodeBuilder::exit_frame() {
    auto &active_procedure_stack = get_active_procedure_stack();
    active_procedure_stack.pop();
}

void BytecodeBuilder::reset() {
    comptime_total_instructions = 0;
    runtime_total_instructions = 0;

    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures.clear();
    procedure_stack = stack<int>();

    int global_id = get_procedure_frame_id("GLOBAL");
    procedures[global_id] = ProcedureFrame("GLOBAL");
    procedure_stack.push(global_id);

    build_puts();
    build_puti();
}

bytecode BytecodeBuilder::finalize_comptime_bytecode(map<int, int> &procedure_line_locations) {
    int total_instruction_count = get_total_instruction_count();
    int neutral_operand_encoding[4] = {0, 0, 0, 0};
    bytecode result;
    result.reserve(total_instruction_count);

    for (const auto &[id, frame]: comptime_procedures) {
        string procedure_name = frame.name;
        if (procedure_name == "GLOBAL") continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id("GLOBAL");
    auto global_instructions = comptime_procedures[global_id].procedure_instructions;
    procedure_line_locations[global_id] = result.size();
    result.insert(result.end(), global_instructions.begin(), global_instructions.end());
    result.push_back(command(VOP_EXIT, -1, neutral_operand_encoding));
    return result;
}

bytecode BytecodeBuilder::finalize_runtime_bytecode(map<int, int> &procedure_line_locations) {
    int total_instruction_count = get_total_instruction_count();
    int neutral_operand_encoding[4] = {0, 0, 0, 0};
    bytecode result;
    result.reserve(total_instruction_count);

    for (const auto &[id, frame]: runtime_procedures) {
        string procedure_name = frame.name;
        if (procedure_name == "GLOBAL") continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id("GLOBAL");
    auto global_instructions = runtime_procedures[global_id].procedure_instructions;

    if (runtime_has_main_function) {
        // interpreter program pointer always starts at procedure_line_locations[id("GLOBAL")]
        // but we want runtime execution to start at "main" function so we point the global starter to the main func line location
        procedure_line_locations[global_id] = procedure_line_locations[get_procedure_frame_id("main")];
    }

    result.insert(result.end(), global_instructions.begin(), global_instructions.end());
    result.push_back(command(VOP_EXIT, -1, neutral_operand_encoding));
    return result;
}

void BytecodeBuilder::build_im(SageOpCode opcode, SageValue op) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    int encoding[4] = {0, 0, 0, 0};
    procedures[procedure_stack.top()].procedure_instructions.push_back(command(opcode, op.as_operand(), encoding));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_im(SageOpCode opcode, SageValue imm1, SageValue imm2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_reg(SageOpCode opcode, SageValue immediate, int reg_index) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        immediate.as_operand(),
        reg_index,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_im(SageOpCode opcode, int reg_index, SageValue immediate) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg_index,
        immediate.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_reg(SageOpCode opcode, int reg1, int reg2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_im_im(SageOpCode opcode, int reg, SageValue imm1, SageValue imm2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg,
        imm1.as_operand(),
        imm2.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_reg_reg(SageOpCode opcode, int reg1, int reg2, int reg3) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 1, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        reg3,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_im_reg(SageOpCode opcode, int reg1, SageValue immediate, int reg2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 0, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        immediate.as_operand(),
        reg2,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_reg_reg_im(SageOpCode opcode, int reg1, int reg2, SageValue immediate) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {1, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        immediate.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_im_im(SageOpCode opcode, SageValue imm1, SageValue imm2, SageValue imm3) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        imm3.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_reg_reg(SageOpCode opcode, SageValue immediate, int reg1, int reg2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 1, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        immediate.as_operand(),
        reg1,
        reg2,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_reg_im(SageOpCode opcode, SageValue imm1, int reg, SageValue imm2) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        reg,
        imm2.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_im_im_reg(SageOpCode opcode, SageValue imm1, SageValue imm2, int reg) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();
    int deref_encoding[4] = {0, 0, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        reg,
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_constpool_im(SageOpCode opcode, int pool_index, SageValue immediate) {
    int deref_encoding[4] = {4, 0, 0, 0}; // 4 = constant pool dereference
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        pool_index,
        immediate.as_operand(),
        deref_encoding
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_puti() {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    int id = get_procedure_frame_id("puti");
    procedures[id] = ProcedureFrame("puti");
    procedure_stack.push(id);
    build_im(OP_LABEL, id);
    build_im_im(OP_MOV, SAGESYS_write_int, 22);
    build_reg_im(OP_MOV, 1, 10); // save digit count (r1) in temp register r10
    build_reg_im(OP_MOV, 0, 1); // save integer to print into r1
    build_reg_im(OP_MOV, 10, 2); // save digit count into r2
    build_im_im(OP_MOV, STDOUT_FILENO, 0); // tell system that this is outputting to stdout
    build_im(OP_SYSCALL, -1);
    build_im(OP_RET, -1);
    procedure_stack.pop();
}

void BytecodeBuilder::build_puts() {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    int id = get_procedure_frame_id("puts");
    procedures[id] = ProcedureFrame("puts");
    procedure_stack.push(id);
    build_im(OP_LABEL, id);
    build_im_im(OP_MOV, SYS_write, 22);
    build_reg_im(OP_MOV, 1, 10);
    build_reg_im(OP_MOV, 0, 1);
    build_reg_im(OP_MOV, 10, 2);
    build_im_im(OP_MOV, STDOUT_FILENO, 0);
    build_im(OP_SYSCALL, -1);
    build_im(OP_RET, -1);
    procedure_stack.pop();
}

ui32 SageCompiler::build_store(ui32 rhs, symbol_entry *var_symbol) {
    symbol_entry *rhs_symbol = symbol_table.lookup_by_index(rhs);

    // is it a variable
    //   is it spilled -> is it stale?
    //   otherwise it has an assigned register (wont be stale)
    // is it a literal expr -> used assigned register
    // is it a literal value
    //     is it stale?

    table_index idx = rhs_symbol->symbol_id;
    if (symbol_table.is_variable(idx) || symbol_table.is_parameter(idx)) {
        if (rhs_symbol->spilled) {
            // load it from stack offset
            int dest_reg = get_volatile();
            builder.build_reg_im(OP_LOAD, dest_reg, rhs_symbol->spill_offset);
            rhs_symbol->assigned_register = dest_reg;
        }
    } else if (!rhs_symbol->value.is_null()) {
        // literal value
        if (var_symbol->spilled) {
            builder.build_im_im(OP_STORE, rhs_symbol->value, var_symbol->spill_offset);
        } else {
            builder.build_im_im(OP_MOV, rhs_symbol->value, var_symbol->assigned_register);
        }
        return 0;
    }

    if (var_symbol->spilled) {
        builder.build_reg_im(OP_STORE, rhs_symbol->assigned_register, var_symbol->spill_offset);
    } else {
        builder.build_reg_im(OP_MOV, rhs_symbol->assigned_register, var_symbol->assigned_register);
    }

    return 0;
}

ui32 SageCompiler::build_return(ui32 return_value_id, bool is_program_exit) {
    // TODO: doesn't yet support multiple return values
    // this is why we are just by default loading the
    // return value into sr6 because sr6 is the first
    // return register.

    SageOpCode retcode = OP_RET;
    if (is_program_exit) {
        retcode = VOP_EXIT;
    }

    if (return_value_id == SAGE_NULL_SYMBOL) {
        builder.build_im(retcode, 0);
        builder.exit_frame();
        return SAGE_NULL_SYMBOL;
    }

    auto return_symbol = symbol_table.lookup_by_index(return_value_id);

    // is it a variable
    //   is it spilled -> is it stale?
    //   otherwise it has an assigned register (wont be stale)
    // is it a literal expr -> used assigned register
    // is it a literal value
    //     is it stale?

    table_index symbol_id = return_symbol->symbol_id;
    bool is_var = (symbol_table.is_variable(symbol_id) || symbol_table.is_parameter(symbol_id));

    if (is_var && return_symbol->spilled) {
        // load it from stack offset
        builder.build_reg_im(OP_LOAD, 6, return_symbol->spill_offset);
        builder.build_im(retcode, 0);
        builder.exit_frame();
    } else if (is_var) {
        builder.build_reg_im(OP_MOV, return_symbol->assigned_register, 6);
    } else if (!return_symbol->value.is_null()) {
        // literal value
        builder.build_im_im(OP_MOV, return_symbol->value, 6);
    } else {
        builder.build_reg_im(OP_MOV, return_symbol->assigned_register, 6);
    }

    builder.build_im(retcode, 0);
    builder.exit_frame();

    return SAGE_NULL_SYMBOL;
}

ui32 SageCompiler::build_function_with_block(string function_name) {
    builder.new_frame(function_name);
    builder.build_im(OP_LABEL, get_procedure_frame_id(function_name));
    return SAGE_NULL_SYMBOL;
}

ui32 SageCompiler::build_alloca(symbol_entry *var_symbol) {
    if (var_symbol->spilled) {
        builder.build_im_im_im(OP_ADD, STACK_POINTER, STACK_POINTER, 1);
    }

    return SAGE_NULL_SYMBOL;
}

ui32 SageCompiler::build_operator(ui32 value1_id, ui32 value2_id, SageOpCode opcode) {
    auto lhs_symbol = symbol_table.lookup_by_index(value1_id);
    auto rhs_symbol = symbol_table.lookup_by_index(value2_id);
    SageValue lhs = SageValue();
    SageValue rhs = SageValue();

    if (lhs_symbol->spilled) {
        // load it from stack offset
        int dest_reg = get_volatile();
        builder.build_reg_im(OP_LOAD, dest_reg, lhs_symbol->spill_offset);
        lhs_symbol->assigned_register = dest_reg;
    } else if (!lhs_symbol->value.is_null()) {
        // literal value
        lhs = lhs_symbol->value;
    }

    if (rhs_symbol->spilled) {
        // load it from stack offset
        int dest_reg = get_volatile();
        builder.build_reg_im(OP_LOAD, dest_reg, lhs_symbol->spill_offset);
        rhs_symbol->assigned_register = dest_reg;
    } else if (!rhs_symbol->value.is_null()) {
        // literal value
        rhs = rhs_symbol->value;
    }

    int result_register = get_volatile();
    if (lhs.is_null() && rhs.is_null()) {
        builder.build_im_reg_reg(opcode, result_register, lhs_symbol->assigned_register, rhs_symbol->assigned_register);
    } else if (lhs.is_null() && !rhs.is_null()) {
        builder.build_im_reg_im(opcode, result_register, lhs_symbol->assigned_register, rhs);
    } else if (!lhs.is_null() && rhs.is_null()) {
        builder.build_im_im_reg(opcode, result_register, lhs, rhs_symbol->assigned_register);
    } else if (!lhs.is_null() && !rhs.is_null()) {
        builder.build_im_im_im(opcode, result_register, lhs, rhs);
    }

    ui32 result_symbol_id = symbol_table.declare_temporary(result_register);
    return result_symbol_id;
}

ui32 SageCompiler::build_add(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_ADD);
}

ui32 SageCompiler::build_sub(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_SUB);
}

ui32 SageCompiler::build_mul(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_MUL);
}

ui32 SageCompiler::build_div(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_DIV);
}

ui32 SageCompiler::build_and(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_AND);
}

ui32 SageCompiler::build_or(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_OR);
}

ui32 SageCompiler::build_load(NodeIndex reference_node) {
    string reference_name = node_manager->get_identifier(reference_node);
    int scope_id = node_manager->get_scope_id(reference_node);
    auto symbol = symbol_table.lookup(reference_name, scope_id);

    if (symbol->spilled) {
        int volatile_reg = get_volatile();
        builder.build_reg_im(OP_LOAD, volatile_reg, symbol->spill_offset);
        symbol->assigned_register = volatile_reg;
    }

    return symbol_table.lookup_table_idx(reference_name, scope_id);
}

ui32 SageCompiler::build_function_call(vector<ui32> args, int table_index) {
    symbol_entry *symbol = symbol_table.lookup_by_index(table_index);
    string function_name = symbol->identifier;
    if (args.size() > 6) {
        logger.log_internal_error_unsafe(
            "builders.cpp",
            current_linenum,
            sen(function_name, "with more than 6 arguments is unimplemented."));
        return SAGE_NULL_SYMBOL;
    }

    symbol_entry *param_symbol;
    for (int i = 0; i < (int) args.size(); ++i) {
        param_symbol = symbol_table.lookup_by_index(args[i]);

        if (param_symbol->spilled) {
            // is either a parameter or variable
            builder.build_reg_im(OP_LOAD, i, param_symbol->spill_offset);
        } else if (symbol_table.is_constant(param_symbol->symbol_id)) {
            // Value is in constant pool - use constant pool dereference
            builder.build_constpool_im(OP_MOV, param_symbol->symbol_id, i);
        } else if (!param_symbol->value.is_null()) {
            builder.build_im_im(OP_MOV, param_symbol->value, i);
        } else {
            builder.build_reg_im(OP_MOV, param_symbol->assigned_register, i);
        }
    }

    builder.build_im(OP_CALL, get_procedure_frame_id(function_name));

    symbol->assigned_register = 6;
    // TODO: (temporary) when we support more than one return value we will need to beef up the logic around this
    return table_index;
}

void SageCompiler::process_escape_sequences(string &str) {
    size_t write_pos = 0;

    for (size_t read_pos = 0; read_pos < str.length(); ++read_pos) {
        if (str[read_pos] == '\\' && read_pos + 1 < str.length()) {
            switch (str[read_pos + 1]) {
                case 'n':
                    str[write_pos++] = '\n';
                    break;
                case 't':
                    str[write_pos++] = '\t';
                    break;
                case 'r':
                    str[write_pos++] = '\r';
                    break;
                case '\\':
                    str[write_pos++] = '\\';
                    break;
                case '"':
                    str[write_pos++] = '"';
                    break;
                case '0':
                    str[write_pos++] = '\0';
                    break;
                default:
                    str[write_pos++] = str[read_pos];
                    continue;
            }
            ++read_pos;
            continue;
        }

        str[write_pos++] = str[read_pos];
    }

    str.resize(write_pos);
}
