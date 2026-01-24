#include "../include/sage_types.h"
#include "../include/registers.h"
#include <error_logger.h>
#include <cstdint>

SageBuiltinType::SageBuiltinType(CanonicalType type, int size, int alignment) {
    this->canonical_type = type;
    this->size = size;
    this->alignment = alignment;
}

CanonicalType SageBuiltinType::identify() {
    return canonical_type;
}

bool SageBuiltinType::match(SageType *other) {
    auto other_identity = other->identify();
    return other_identity == this->identify();
}

string SageBuiltinType::to_string() {
    switch (canonical_type) {
        case BOOL:
            return "bool";
        case CHAR:
            return "char";
        case INT:
            return str("i", size*8);
        case FLOAT:
            return str("f", size*8);
        case VOID:
            return "void";
        default:
            return "<unknown>";
    }
}

SagePointerType::SagePointerType(SageType *pointee) : pointer_type(pointee) {
    this->size = 4;
    this->alignment = 4;
}

CanonicalType SagePointerType::identify() {
    return POINTER;
}

bool SagePointerType::match(SageType *other) {
    auto otherident = other->identify();
    if (otherident != POINTER) {
        return false;
    }

    SagePointerType *other_pointer = dynamic_cast<SagePointerType *>(other);
    return pointer_type->match(other_pointer->pointer_type);
}

string SagePointerType::to_string() {
    return str(pointer_type->to_string(), "*");
}

SageArrayType::SageArrayType(SageType *element_type, int length) : array_type(element_type) {
    this->size = length * element_type->size;
    this->alignment = element_type->alignment;
}

CanonicalType SageArrayType::identify() {
    return ARRAY;
}

bool SageArrayType::match(SageType *other) {
    auto otherident = other->identify();
    if (otherident != ARRAY) {
        return false;
    }

    SageArrayType *other_array = dynamic_cast<SageArrayType *>(other);
    return (size != other_array->size) && array_type->match(other_array->array_type);
}

string SageArrayType::to_string() {
    return str(array_type->to_string(), "[", length, "]");
}

SageFunctionType::SageFunctionType(vector<SageType *> returns, vector<SageType *> params) : return_type(returns),
    parameter_types(params) {
    this->size = 0;
    this->alignment = 0;
}

CanonicalType SageFunctionType::identify() {
    return FUNC;
}

bool SageFunctionType::match(SageType *other) {
    if (other->identify() != FUNC) {
        return false;
    }

    SageFunctionType *functype = dynamic_cast<SageFunctionType *>(other);
    if (functype->parameter_types.size() != parameter_types.size() || functype->return_type.size() != return_type.
        size()) {
        return false;
    }

    for (int i = 0; i < (int) parameter_types.size(); ++i) {
        if (!functype->parameter_types[i]->match(parameter_types[i])) {
            return false;
        }
    }

    for (int i = 0; i < (int) return_type.size(); ++i) {
        if (!functype->return_type[i]->match(return_type[i])) {
            return false;
        }
    }

    return true;
}

string SageFunctionType::to_string() {
    string value = "(";
    for (int i = 0; i < (int) parameter_types.size(); ++i) {
        value += parameter_types[i]->to_string();
        if (i != (int) parameter_types.size() - 1) {
            value += ",";
        } else if (i == (int) parameter_types.size() - 1) {
            value += ") ";
            break;
        }

        value += " ";
    }

    value += "-> ";

    for (int i = 0; i < (int) return_type.size(); ++i) {
        value += return_type[i]->to_string();
        if (i != (int) return_type.size() - 1) {
            value += ",";
        } else if (i == (int) return_type.size() - 1) {
            break;
        }

        value += " ";
    }
    return value;
}

SageStructType::SageStructType(string name, vector<SageType *> member_types, int size, int alignment) : name(name),
    member_types(member_types) {
    this->size = size;
    this->alignment = alignment;
}

CanonicalType SageStructType::identify() {
    return CUSTOM;
}

bool SageStructType::match(SageType *other) {
    if (other->identify() != CUSTOM) return false;

    SageStructType *other_struct = dynamic_cast<SageStructType *>(other);

    int lowest_member_length = (other_struct->member_types.size() > member_types.size())
                                          ? member_types.size()
                                          : other_struct->member_types.size();
    SageType *other_current_member;
    SageType *self_current_member;
    for (int i = 0; i < lowest_member_length; ++i) {
        other_current_member = other_struct->member_types[i];
        self_current_member = member_types[i];
        if (!self_current_member->match(other_current_member)) {
            return false;
        }
    }
    return true;
}

string SageStructType::to_string() {
    return name;
}

SageValue::SageValue(int _value, SageType *valuetype) : valuetype(valuetype) {
    value.int_value = _value;
    nullvalue = false;
}

SageValue::SageValue(float _value, SageType *valuetype) : valuetype(valuetype) {
    value.float_value = _value;
    nullvalue = false;
}

SageValue::SageValue(char _value, SageType *valuetype) : valuetype(valuetype) {
    value.char_value = _value;
    nullvalue = false;
}

SageValue::SageValue(bool _value, SageType *valuetype) : valuetype(valuetype) {
    value.bool_value = _value;
    nullvalue = false;
}

SageValue::SageValue(void *_value, SageType *valuetype) : valuetype(valuetype) {
    value.complex_value = _value;
    nullvalue = false;
}

SageValue::SageValue(int _value) : valuetype(TypeRegistery::get_integer_type(8)) {
    value.int_value = _value;
    nullvalue = false;
}

SageValue::SageValue(float _value) : valuetype(TypeRegistery::get_float_type(8)) {
    value.float_value = _value;
    nullvalue = false;
}

// Constructor from register
SageValue::SageValue(ui64 register_value) {
    RegType reg_type = unpack_type(register_value);
    nullvalue = false;

    switch (reg_type) {
        case I32_REG:
            value.int_value = unpack_int(register_value);
            valuetype = TypeRegistery::get_integer_type(4);
            break;
        case F32_REG:
            value.float_value = unpack_float(register_value);
            valuetype = TypeRegistery::get_float_type(4);
            break;
        case PTR_REG:
            value.complex_value = unpack_pointer(register_value);
            valuetype = TypeRegistery::get_pointer_type(TypeRegistery::get_byte_type(VOID));
            break;
    }
}

SageValue::SageValue() {
}

SageValue::~SageValue() {
}

// For instruction operands
int SageValue::as_operand() const {
    switch (valuetype->identify()) {
        case INT:
            return value.int_value;
        case FLOAT:
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
        case INT:
            return pack_int(value.int_value);
        case FLOAT:
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

bool SageValue::equals(const SageValue &other) {
    auto this_type = valuetype->identify();
    switch (this_type) {
        case CHAR:
            return value.char_value == other.value.char_value && this_type == other.valuetype->identify();
        case BOOL:
            return value.bool_value == other.value.bool_value && this_type == other.valuetype->identify();
        case INT:
            return value.int_value == other.value.int_value && this_type == other.valuetype->identify();
        case FLOAT:
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
SageType *TypeRegistery::get_builtin_type(CanonicalType canonical_type, int bytesize) {
    auto key = std::make_pair(canonical_type, bytesize);
    auto it = builtin_types.find(key);
    if (it != builtin_types.end()) return it->second.get();

    auto type = std::make_unique<SageBuiltinType>(canonical_type, bytesize, bytesize);
    builtin_types[key] = std::move(type);
    return builtin_types[key].get();
}

SageType *TypeRegistery::get_integer_type(int bytesize) {
    auto key = std::make_pair(INT, bytesize);
    auto it = builtin_types.find(key);
    if (it != builtin_types.end()) return it->second.get();

    auto type = std::make_unique<SageBuiltinType>(INT, bytesize, bytesize);
    builtin_types[key] = std::move(type);
    return builtin_types[key].get();
}

SageType *TypeRegistery::get_float_type(int bytesize) {
    auto key = std::make_pair(FLOAT, bytesize);
    auto it = builtin_types.find(key);
    if (it != builtin_types.end()) return it->second.get();

    auto type = std::make_unique<SageBuiltinType>(FLOAT, bytesize, bytesize);
    builtin_types[key] = std::move(type);
    return builtin_types[key].get();
}

SageType *TypeRegistery::get_byte_type(CanonicalType canonical_type) {
    if (canonical_type != VOID && canonical_type != CHAR && canonical_type != BOOL) return nullptr;

    auto key = std::make_pair(canonical_type, 1);
    auto it = builtin_types.find(key);
    if (it != builtin_types.end()) return it->second.get();

    auto type = std::make_unique<SageBuiltinType>(canonical_type, 1, 1);
    builtin_types[key] = std::move(type);
    return builtin_types[key].get();
}

SageType *TypeRegistery::get_pointer_type(SageType *base_type) {
    auto it = pointer_types.find(base_type);
    if (it != pointer_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SagePointerType>(base_type);
    pointer_types[base_type] = std::move(type);
    return pointer_types[base_type].get();
}

SageType *TypeRegistery::get_array_type(SageType *element_type, int length) {
    auto key = std::make_pair(element_type, length);
    auto it = array_types.find(key);
    if (it != array_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageArrayType>(element_type, length);
    array_types[key] = std::move(type);
    return array_types[key].get();
}

SageType *TypeRegistery::get_struct_type(string name, std::vector<SageType *> member_types) {
    auto it = struct_types.find(name);
    if (it != struct_types.end()) {
        return it->second.get();
    }

    int largest_alignment = 0;
    int size = 0;
    for (auto member: member_types) {
        if (member->alignment > largest_alignment) largest_alignment = member->alignment;
        size += member->size;
    }

    auto type = std::make_unique<SageStructType>(name, member_types, size, largest_alignment);
    struct_types[name] = std::move(type);
    return struct_types[name].get();
}

SageType *TypeRegistery::get_function_type(std::vector<SageType *> return_types,
                                           std::vector<SageType *> parameter_types) {
    auto key = std::make_pair(return_types, parameter_types);
    auto it = function_types.find(key);
    if (it != function_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageFunctionType>(return_types, parameter_types);
    function_types[key] = std::move(type);
    return function_types[key].get();
}
