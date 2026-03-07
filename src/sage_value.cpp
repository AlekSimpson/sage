#include <algorithm>
#include <cstring>
#include <cassert>
#include "../include/sage_value.h"
#include "../include/error_logger.h"

SageValue::SageValue(char value) : type(TypeRegistery::get_byte_type(CHAR)), nullvalue(false) {
    byte_data = new Byte[1];
    std::memcpy(byte_data, &value, sizeof(char));
}
SageValue::SageValue(bool value) : type(TypeRegistery::get_byte_type(BOOL)), nullvalue(false) {
    byte_data = new Byte[1];
    std::memcpy(byte_data, &value, sizeof(bool));
}
SageValue::SageValue(int32_t value) : type(TypeRegistery::get_integer_type(4)), nullvalue(false) {
    byte_data = new Byte[4];
    std::memcpy(byte_data, &value, sizeof(int32_t));
}
SageValue::SageValue(int64_t value) : type(TypeRegistery::get_integer_type(8)), nullvalue(false) {
    byte_data = new Byte[8];
    std::memcpy(byte_data, &value, sizeof(int64_t));
}
SageValue::SageValue(float value) : type(TypeRegistery::get_float_type(4)), nullvalue(false) {
    byte_data = new Byte[4];
    std::memcpy(byte_data, &value, sizeof(float));
}
SageValue::SageValue(double value) : type(TypeRegistery::get_float_type(8)), nullvalue(false) {
    byte_data = new Byte[8];
    std::memcpy(byte_data, &value, sizeof(double));
}
SageValue::SageValue(SageType *custom_type) : type(custom_type), nullvalue(false) {
    byte_data = new Byte[custom_type->size];
    SageStructType *struct_type = static_cast<SageStructType *>(custom_type);
    int pointer = 0;
    SageValue default_value;
    for (auto member_type: struct_type->member_types) {
        default_value = member_type->get_default_value();
        std::memcpy(byte_data + pointer, default_value.byte_data, default_value.type->size);
        pointer += default_value.type->size;
    }
}
SageValue::SageValue(SageType *custom_type, ByteVector init_data) : type(custom_type), nullvalue(false) {
    assert(custom_type->size == (int)init_data.size());
    byte_data = new Byte[custom_type->size];
    std::memcpy(byte_data, init_data.data(), custom_type->size);
}

SageValue::SageValue(): nullvalue(true) {}

SageValue::~SageValue() {
    if (byte_data != nullptr) delete[] byte_data;
}

// Copy constructor
SageValue::SageValue(const SageValue& other)
    : type(other.type), nullvalue(other.nullvalue) {
    if (other.byte_data != nullptr && other.type != nullptr) {
        byte_data = new Byte[other.type->size];
        std::memcpy(byte_data, other.byte_data, other.type->size);
    } else {
        byte_data = nullptr;
    }
}

// Copy assignment
SageValue& SageValue::operator=(const SageValue& other) {
    if (this != &other) {
        delete[] byte_data;
        type = other.type;
        nullvalue = other.nullvalue;
        if (other.byte_data != nullptr && other.type != nullptr) {
            byte_data = new Byte[other.type->size];
            std::memcpy(byte_data, other.byte_data, other.type->size);
        } else {
            byte_data = nullptr;
        }
    }
    return *this;
}

// Move constructor
SageValue::SageValue(SageValue&& other) noexcept
    : byte_data(other.byte_data), type(other.type), nullvalue(other.nullvalue) {
    other.byte_data = nullptr;
}

// Move assignment
SageValue& SageValue::operator=(SageValue&& other) noexcept {
    if (this != &other) {
        delete[] byte_data;
        byte_data = other.byte_data;
        type = other.type;
        nullvalue = other.nullvalue;
        other.byte_data = nullptr;
    }
    return *this;
}

bool SageValue::is_null() {
    return nullvalue;
}

bool SageValue::equals(const SageValue &other) {
    if (!type->match(other.type)) return false;

    int j = type->size-1;
    for (int i = 0; i < type->size && i < j; ++i) {
        if (other.byte_data[i] != byte_data[i]) return false;
        if (other.byte_data[j] != byte_data[j]) return false;
        j--;
    }

    return true;
}

int32_t SageValue::as_i32() {
    int32_t result = 0;
    std::memcpy(&result, byte_data, sizeof(int32_t));
    return result;
}

int64_t SageValue::as_i64() {
    int64_t result = 0;
    std::memcpy(&result, byte_data, std::min((size_t)type->size, sizeof(int64_t)));
    return result;
}

uint32_t SageValue::as_u32() {
    uint32_t result = 0;
    std::memcpy(&result, byte_data, sizeof(int32_t));
    return result;
}

uint64_t SageValue::as_u64() {
    uint64_t result = 0;
    std::memcpy(&result, byte_data, std::min((size_t)type->size, sizeof(uint64_t)));
    return result;
}

float SageValue::as_f32() {
    float result = 0;
    std::memcpy(&result, byte_data, sizeof(float));
    return result;
}

double SageValue::as_f64() {
    double result = 0;
    std::memcpy(&result, byte_data, sizeof(double));
    return result;
}

bool SageValue::as_bool() {
    bool result = 0;
    std::memcpy(&result, byte_data, sizeof(bool));
    return result;
}

char SageValue::as_char() {
    char result = '0';
    std::memcpy(&result, byte_data, sizeof(char));
    return result;
}