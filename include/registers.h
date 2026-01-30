#pragma once

#include "sage_types.h"

#define ui64 uint64_t

enum RegType { I32_REG = 0 };
constexpr uint64_t TYPE_MASK = 0x7;  // Lower 3 bits

ui64 pack_int(int32_t value);
int32_t unpack_int(ui64 reg);
int unpack_int64(ui64 reg);
ui64 int_reg_inc(ui64 reg, int increment);
ui64 int_reg_add(ui64 reg_a, ui64 reg_b);
ui64 int_reg_sub(ui64 reg_a, ui64 reg_b);









