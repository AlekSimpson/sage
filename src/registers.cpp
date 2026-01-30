#include "../include/sage_types.h"
#include "../include/registers.h"

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
