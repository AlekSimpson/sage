#include "../include/sage_types.h"
#include "../include/registers.h"
#include <error_logger.h>
#include <cstdint>

SageBuiltinType::SageBuiltinType(CanonicalType type) {
    canonical_type = type;
}

CanonicalType SageBuiltinType::identify() {
    return canonical_type;
}

bool SageBuiltinType::match(SageType* other) {
    auto other_identity = other->identify();
    return other_identity == this->identify();
}

string SageBuiltinType::to_string() {
    switch (canonical_type) {
        case BOOL:
            return "bool";
        case CHAR:
            return "char";
        case I8:
            return "i8";
        case I32:
            return "i32";
        case I64:
            return "i64";
        case F32:
            return "f32";
        case F64:
            return "f64";
        case VOID:
            return "void";
        default:
            return "<unknown>";
    }
}

SagePointerType::SagePointerType(SageType* pointee) : pointer_type(pointee) {}

CanonicalType SagePointerType::identify() {
    return POINTER;
}

bool SagePointerType::match(SageType* other) {
    auto otherident = other->identify();
    if (otherident != POINTER) {
        return false;
    }

    SagePointerType* other_pointer = dynamic_cast<SagePointerType*>(other);
    return pointer_type->match(other_pointer->pointer_type);
}

string SagePointerType::to_string() {
    return str(pointer_type->to_string(), "*");
}

SageArrayType::SageArrayType(SageType* element_type, int size) : array_type(element_type), size(size) {}

CanonicalType SageArrayType::identify() {
    return ARRAY;
}

bool SageArrayType::match(SageType* other) {
    auto otherident = other->identify();
    if (otherident != ARRAY) {
        return false;
    }

    SageArrayType* other_array = dynamic_cast<SageArrayType*>(other);
    return (size != other_array->size) && array_type->match(other_array->array_type);
}

string SageArrayType::to_string() {
    return str(array_type->to_string(), "[]");
}

SageFunctionType::SageFunctionType(vector<SageType*> returns, vector<SageType*> params) :
    return_type(returns), parameter_types(params) {}

CanonicalType SageFunctionType::identify() {
    return FUNC;
}

bool SageFunctionType::match(SageType* other) {
    if (other->identify() != FUNC) {
        return false;
    }
    
    SageFunctionType* functype = dynamic_cast<SageFunctionType*>(other);
    if (functype->parameter_types.size() != parameter_types.size() || functype->return_type.size() != return_type.size()) {
        return false;
    }

    for (int i = 0; i < (int)parameter_types.size(); ++i) {
        if (!functype->parameter_types[i]->match(parameter_types[i])) {
            return false;
        }
    }

    for (int i = 0; i < (int)return_type.size(); ++i) {
        if (!functype->return_type[i]->match(return_type[i])) {
            return false;
        }
    }

    return true;
}

string SageFunctionType::to_string() {
    string value = "(";
    for (int i = 0; i < (int)parameter_types.size(); ++i) {
        value += parameter_types[i]->to_string();
        if (i != (int)parameter_types.size() - 1) {
            value += ",";
        }else if (i == (int)parameter_types.size() - 1) {
            value += ") ";
            break;
        }

        value += " ";
    }

    value += "-> ";

    for (int i = 0; i < (int)return_type.size(); ++i) {
        value += return_type[i]->to_string();
        if (i != (int)return_type.size() - 1) {
            value += ",";
        }else if (i == (int)return_type.size() - 1) {
            break;
        }

        value += " ";
    }
    return value;
}

SageValue::SageValue(int size, int _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.int_value = _value;
    nullvalue = false;
}

SageValue::SageValue(int size, float _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.float_value = _value;
    nullvalue = false;
}

SageValue::SageValue(int size, char _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.char_value = _value;
    nullvalue = false;
}

SageValue::SageValue(int size, bool _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.bool_value = _value;
    nullvalue = false;
}

// SageValue::SageValue(int size, string* _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
//     value.string_value = _value;
//     nullvalue = false;
// }

SageValue::SageValue(int size, void* _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.complex_value = _value;
    nullvalue = false;
}

SageValue::SageValue(int _value) : bitsize(64), valuetype(TypeRegistery::get_builtin_type(I64)) {
    value.int_value = _value;
    nullvalue = false;
}

SageValue::SageValue(float _value) : bitsize(64), valuetype(TypeRegistery::get_builtin_type(F64)) {
    value.float_value = _value;
    nullvalue = false;
}

// Constructor from register
SageValue::SageValue(ui64 register_value) {
    RegType reg_type = unpack_type(register_value);
    nullvalue = false;

    switch(reg_type) {
        case I32_REG:
            bitsize = 32;
        value.int_value = unpack_int(register_value);
        valuetype = TypeRegistery::get_builtin_type(I32);
        break;
        case F32_REG:
            bitsize = 32;
        value.float_value = unpack_float(register_value);
        valuetype = TypeRegistery::get_builtin_type(F32);
        break;
        case PTR_REG:
            bitsize = 64;
        value.complex_value = unpack_pointer(register_value);
        valuetype = TypeRegistery::get_builtin_type(POINTER);
        break;
    }
}

SageValue::SageValue() {}
SageValue::~SageValue() {}

// For instruction operands
int SageValue::as_operand() const {
    switch(valuetype->identify()) {
        case I32:
        case I8:
        case I64:
            return value.int_value;
        case F32:
        case F64:
            return static_cast<int>(value.float_value);
        case CHAR:
            return value.char_value;
        case BOOL:
            return value.bool_value;
        case POINTER:
        case ARRAY:
            return static_cast<int>(reinterpret_cast<uintptr_t>(value.complex_value));
        default:
            return 0;
    }
}

uint64_t SageValue::load() {
    switch (valuetype->identify()) {
        case CHAR:
            return pack_int(value.char_value);
        case BOOL:
            return pack_int(value.bool_value);
        case I8:
        case I32:
        case I64:
            return pack_int(value.int_value);
        case F32:
        case F64:
            return pack_float(value.float_value);
        case POINTER:
        case ARRAY:
            return pack_ptr(value.complex_value);
        case FUNC:
            break;
        default:
            break;
    }
    ErrorLogger::get().log_internal_error(
        "sage_types.cpp",
        current_linenum,
        "unimplemented load() for function types");

    return 0;
}

bool SageValue::is_null() {
    return nullvalue;
}

bool SageValue::equals(const SageValue& other) {
    auto this_type = valuetype->identify();
    switch (this_type) {
        case CHAR:
            return value.char_value == other.value.char_value && this_type == other.valuetype->identify();
        case BOOL:
            return value.bool_value == other.value.bool_value && this_type == other.valuetype->identify();
        case I8:
        case I32:
        case I64:
            return value.int_value == other.value.int_value && this_type == other.valuetype->identify();
        case F32:
        case F64:
            return value.float_value == other.value.float_value && this_type == other.valuetype->identify();
        case POINTER:
        case ARRAY:
            return value.complex_value == other.value.complex_value && this_type == other.valuetype->identify();
        case FUNC:
            break;
        default:
            break;
    }
    ErrorLogger::get().log_internal_error(
       "sage_types.cpp",
       current_linenum,
       "Unknown sage type encountered");

    return false;
}


/// Type Registery
SageType* TypeRegistery::get_builtin_type(CanonicalType canonical_type) {
    auto it = builtin_types.find(canonical_type);
    if (
        it != builtin_types.end()) {
        return it->second.get();
    }
    
    auto type = std::make_unique<SageBuiltinType>(canonical_type);
    SageType* ptr = type.get();
    builtin_types[canonical_type] = std::move(type);
    return ptr;
}

SageType* TypeRegistery::get_pointer_type(SageType* base_type) {
    auto it = pointer_types.find(base_type);
    if (it != pointer_types.end()) {
        return it->second.get();
    }
    
    auto type = std::make_unique<SagePointerType>(base_type);
    SageType* ptr = type.get();
    pointer_types[base_type] = std::move(type);
    return ptr;
}

SageType* TypeRegistery::get_array_type(SageType* element_type, int size) {
    auto key = std::make_pair(element_type, size);
    auto it = array_types.find(key);
    if (it != array_types.end()) {
        return it->second.get();
    }
    
    auto type = std::make_unique<SageArrayType>(element_type, size);
    SageType* ptr = type.get();
    array_types[key] = std::move(type);
    return ptr;
}

SageType* TypeRegistery::get_function_type(std::vector<SageType*> return_types, std::vector<SageType*> parameter_types) {
    auto key = std::make_pair(return_types, parameter_types);
    auto it = function_types.find(key);
    if (it != function_types.end()) {
        return it->second.get();
    }
    
    auto type = std::make_unique<SageFunctionType>(return_types, parameter_types);
    SageType* ptr = type.get();
    function_types[key] = std::move(type);
    return ptr;
}





