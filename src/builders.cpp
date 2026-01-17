#include <unistd.h>
#include <sys/syscall.h>

#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_bytecode.h"
#include "../include/bytecode_builder.h"

std::unordered_map<CanonicalType, std::unique_ptr<SageType> > TypeRegistery::builtin_types;
std::unordered_map<SageType *, std::unique_ptr<SageType> > TypeRegistery::pointer_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::array_types;
std::unordered_map<std::pair<std::vector<SageType *>, std::vector<SageType *> >, std::unique_ptr<SageType> >
TypeRegistery::function_types;

int hash_djb2(const std::string &str) {
    uint64_t hash = 5381;
    for (char c: str) {
        hash = ((hash << 5) + hash) + c;
    }
    return static_cast<int>(hash);
}

BytecodeBuilder::BytecodeBuilder() {
    build_puts();
    build_puti();
}

void BytecodeBuilder::new_frame(string name) {
    auto frame = procedure_frame(name);
    if (name == "main") {
        has_main_function = true;
    }
    int id = hash_djb2(name);
    procedures[id] = frame;
    procedure_stack.push(id);
}

void BytecodeBuilder::exit_frame() {
    procedure_stack.pop();
}

void BytecodeBuilder::reset() {
    procedures.clear();
    procedure_stack = stack<int>();

    build_puts();
    build_puti();
}

bytecode BytecodeBuilder::final(map<int, int> &proc_line_locations, SageSymbolTable *table, bool is_comptime) {
    bytecode result;

    if (!has_main_function) {
        if (!is_comptime) {
            result.reserve(total_instruction_count + 2);

            int encoding[4] = {0, 0, 0, 0};
            result.push_back(command(OP_LABEL, hash_djb2("main"), encoding));
            result.push_back(command(VOP_EXIT, -1, encoding));
        }
    } else {
        result.reserve(total_instruction_count);
        auto hash = hash_djb2("main");
        bytecode &current_instructions = procedures[hash].procedure_instructions;
        result.insert(
            result.end(),
            current_instructions.begin(), current_instructions.end());
    }
    proc_line_locations[hash_djb2("main")] = 0;

    for (const auto &[id, frame]: procedures) {
        string pname = frame.name;
        if (pname == "main") continue;

        bytecode current_instructions = frame.procedure_instructions;
        proc_line_locations[id] = result.size();

        result.insert(
            result.end(),
            current_instructions.begin(), current_instructions.end());
        int enc[4] = {0, 0, 0, 0};
        result.push_back(command(VOP_EXIT, -1, enc));
    }

    for (int builtin: table->builtins) {
        string pname = table->entries[builtin].identifier;
        if (pname == "main") continue;

        bytecode &current_instructions = procedures[hash_djb2(pname)].procedure_instructions;
        proc_line_locations[hash_djb2(pname)] = result.size();

        result.insert(
            result.end(),
            current_instructions.begin(), current_instructions.end());
    }
    return result;
}

void BytecodeBuilder::build_im(SageOpCode opcode, SageValue op) {
    int encoding[4] = {0, 0, 0, 0};
    procedures[procedure_stack.top()].procedure_instructions.push_back(command(opcode, op.as_operand(), encoding));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_im(SageOpCode opcode, SageValue imm1, SageValue imm2) {
    int deref_encoding[4] = {0, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_reg(SageOpCode opcode, SageValue immediate, int reg_index) {
    int deref_encoding[4] = {0, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        immediate.as_operand(),
        reg_index,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_im(SageOpCode opcode, int reg_index, SageValue immediate) {
    int deref_encoding[4] = {1, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg_index,
        immediate.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_reg(SageOpCode opcode, int reg1, int reg2) {
    int deref_encoding[4] = {1, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_im_im(SageOpCode opcode, int reg, SageValue imm1, SageValue imm2) {
    int deref_encoding[4] = {1, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg,
        imm1.as_operand(),
        imm2.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_reg_reg(SageOpCode opcode, int reg1, int reg2, int reg3) {
    int deref_encoding[4] = {1, 1, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        reg3,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_im_reg(SageOpCode opcode, int reg1, SageValue immediate, int reg2) {
    int deref_encoding[4] = {1, 0, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        immediate.as_operand(),
        reg2,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_reg_reg_im(SageOpCode opcode, int reg1, int reg2, SageValue immediate) {
    int deref_encoding[4] = {1, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        reg1,
        reg2,
        immediate.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_im_im(SageOpCode opcode, SageValue imm1, SageValue imm2, SageValue imm3) {
    int deref_encoding[4] = {0, 0, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        imm3.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_reg_reg(SageOpCode opcode, SageValue immediate, int reg1, int reg2) {
    int deref_encoding[4] = {0, 1, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        immediate.as_operand(),
        reg1,
        reg2,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_reg_im(SageOpCode opcode, SageValue imm1, int reg, SageValue imm2) {
    int deref_encoding[4] = {0, 1, 0, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        reg,
        imm2.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_im_im_reg(SageOpCode opcode, SageValue imm1, SageValue imm2, int reg) {
    int deref_encoding[4] = {0, 0, 1, 0};

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        imm1.as_operand(),
        imm2.as_operand(),
        reg,
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_constpool_im(SageOpCode opcode, int pool_index, SageValue immediate) {
    int deref_encoding[4] = {4, 0, 0, 0}; // 4 = constant pool dereference

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        pool_index,
        immediate.as_operand(),
        deref_encoding
    ));
    total_instruction_count++;
}

void BytecodeBuilder::build_puti() {
    //procs.push_back("puti");
    procedures[hash_djb2("puti")] = procedure_frame("puti");
    procedure_stack.push(hash_djb2("puti"));
    build_im(OP_LABEL, hash_djb2("puti"));
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
    //procs.push_back("puts");
    procedures[hash_djb2("puts")] = procedure_frame("puts");
    procedure_stack.push(hash_djb2("puts"));
    build_im(OP_LABEL, hash_djb2("puts"));
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
    builder.build_im(OP_LABEL, hash_djb2(function_name));
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
        logger.log_internal_error(
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

    builder.build_im(OP_CALL, hash_djb2(function_name));

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
