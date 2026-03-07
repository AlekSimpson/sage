#pragma once

#include <cstdint>
#include "sage_types.h"

typedef vector<uint8_t> ByteVector;
typedef uint8_t Byte;

class SageValue {
public:
    Byte* byte_data = nullptr;
    SageType *type = TypeRegistery::get_byte_type(VOID);
    bool nullvalue;

    SageValue();
    SageValue(int32_t);
    SageValue(int64_t);
    SageValue(float);
    SageValue(double);
    SageValue(char);
    SageValue(bool);
    SageValue(SageType * complex_type);
    SageValue(SageType * complex_type, ByteVector init_data);
    ~SageValue();

    // Rule of 5: copy/move semantics
    SageValue(const SageValue& other);
    SageValue& operator=(const SageValue& other);
    SageValue(SageValue&& other) noexcept;
    SageValue& operator=(SageValue&& other) noexcept;

    bool is_null();

    bool equals(const SageValue &other);

    operator double() { return as_f64(); }
    operator int64_t() { return as_i64(); }

    int32_t as_i32();
    int64_t as_i64();
    uint32_t as_u32();
    uint64_t as_u64();
    float as_f32();
    double as_f64();
    bool as_bool();
    char as_char();
};
