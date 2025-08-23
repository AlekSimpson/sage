#pragma once

#include "sage_types.h"

#define ui64 uint64_t

enum RegType { I32_REG = 0, F32_REG = 1, PTR_REG = 2 };
constexpr uint64_t TYPE_MASK = 0x7;  // Lower 3 bits

union floatpacker {
    float float_value;
    uint32_t bits;
};

RegType unpack_type(ui64);

ui64 pack_int(int32_t value);
int32_t unpack_int(ui64 reg);
ui64 int_reg_inc(ui64 reg, int increment);
ui64 int_reg_add(ui64 reg_a, ui64 reg_b);
ui64 int_reg_sub(ui64 reg_a, ui64 reg_b);

ui64 pack_float(float value);
float unpack_float(uint64_t reg);
ui64 float_reg_inc(ui64 reg, float increment);
ui64 float_reg_add(ui64 reg_a, ui64 reg_b);
ui64 float_reg_sub(ui64 reg_a, ui64 reg_b);

ui64 pack_ptr(void* value);
void* unpack_pointer(ui64 reg);

SageValue register_to_value(ui64);









