#include "../include/sage_types.h"
#include "../include/registers.h"

RegType unpack_type(ui64 reg) {
    return static_cast<RegType>(reg & TYPE_MASK);
}

// int register functions

ui64 pack_int(int32_t value) {
    return (static_cast<ui64>(value) << 32) | I32_REG;
}

int32_t unpack_int(ui64 reg) {
    return static_cast<int32_t>(reg >> 32);
}

int unpack_int64(ui64 reg) {
    return static_cast<int>(reg >> 32);
}

ui64 int_reg_inc(ui64 reg, int increment) {
    int32_t value = unpack_int(reg);
    ui64 packed = pack_int(value + increment);
    return packed; // result register value
}

ui64 int_reg_add(ui64 reg_a, ui64 reg_b) {
    int32_t value_a = unpack_int(reg_a);
    int32_t value_b = unpack_int(reg_b);
    ui64 packed = pack_int(value_a + value_b);
    return packed; // result register value
}

ui64 int_reg_sub(ui64 reg_a, ui64 reg_b) {
    int32_t value_a = unpack_int(reg_a);
    int32_t value_b = unpack_int(reg_b);
    ui64 packed = pack_int(value_a - value_b);
    return packed; // result register value
}

// float register functions

ui64 pack_float(float value) {
    floatpacker packer;
    packer.float_value = value;
    return (static_cast<ui64>(packer.bits) << 32) | F32_REG;
}

float unpack_float(uint64_t reg) {
    floatpacker packer;
    packer.bits = static_cast<uint32_t>(reg >> 32);
    return packer.float_value;
}

ui64 float_reg_inc(ui64 reg, float increment) {
    float value = unpack_float(reg);
    ui64 packed = pack_float(value + increment);
    return packed;
}

ui64 float_reg_add(ui64 reg_a, ui64 reg_b) {
    float value_a = unpack_float(reg_a);
    float value_b = unpack_float(reg_b);
    ui64 packed = pack_float(value_a + value_b);
    return packed;
}

ui64 float_reg_sub(ui64 reg_a, ui64 reg_b) {
    float value_a = unpack_float(reg_a);
    float value_b = unpack_float(reg_b);
    ui64 packed = pack_float(value_a - value_b);
    return packed;
}

// pointer register functions

ui64 pack_ptr(void *value) {
    return reinterpret_cast<uintptr_t>(value) | PTR_REG;
}

void *unpack_pointer(ui64 reg) {
    return reinterpret_cast<void *>(reg & ~TYPE_MASK);
}

SageValue register_to_value(ui64 reg) {
    // FIX: need to figure out how to properly unload the contents of a register into a SageValue
    RegType register_type = unpack_type(reg);

    switch (register_type) {
        case I32_REG: {
            int32_t value = unpack_int(reg);
            return SageValue(32, value, TypeRegistery::get_builtin_type(I32));
        }
        case F32_REG: {
            float value = unpack_float(reg);
            return SageValue(32, value, TypeRegistery::get_builtin_type(F32));
        }
        case PTR_REG: {
            void *value = unpack_pointer(reg);
            // NOTE: not sure if we are really unpacking the type here correctly, theoretically this void* could be holding any kind of type but maybe its fine? We'll probably have to do some testing to see how it plays out
            return SageValue(64, value, TypeRegistery::get_builtin_type(POINTER));
        }
        default:
            return SageValue();
    }
}
