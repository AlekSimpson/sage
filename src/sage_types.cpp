#include "../include/sage_types.h"
#include <error_logger.h>

// builtin_type
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

bool SageBuiltinType::is_array() {return false;}
bool SageBuiltinType::is_pointer() {return false;}
bool SageBuiltinType::is_struct() {return false;}
bool SageBuiltinType::is_function() {return false;}

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

// pointer_type
SagePointerType::SagePointerType(SageType *pointee) : pointer_type(pointee) {
    this->size = 8;
    this->alignment = 8;
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

bool SagePointerType::is_array() {return false;}
bool SagePointerType::is_pointer() {return true;}
bool SagePointerType::is_struct() {return false;}
bool SagePointerType::is_function() {return false;}

string SagePointerType::to_string() {
    return str(pointer_type->to_string(), "*");
}

// dynamic_array_type
SageDynamicArrayType::SageDynamicArrayType(SageType *basetype) {
    array_type = basetype;
    length = 0;
    capacity = 15;

    size = capacity * basetype->size;
    alignment = basetype->alignment;
}

SageDynamicArrayType::SageDynamicArrayType(SageType *basetype, int length): length(length), capacity(int(1.5 * (float)length)) {
    array_type = basetype;

    size = capacity * basetype->size;
    alignment = basetype->alignment;
}

CanonicalType SageDynamicArrayType::identify() {
    return DYN_ARRAY;
}

bool SageDynamicArrayType::match(SageType *other) {
    return other->identify() == DYN_ARRAY && dynamic_cast<SageDynamicArrayType *>(other)->array_type->identify() == array_type->identify();
}

bool SageDynamicArrayType::is_array() {
    return true;
}

bool SageDynamicArrayType::is_pointer() {
    return false;
}

bool SageDynamicArrayType::is_struct() {
    return false;
}

bool SageDynamicArrayType::is_function() {
    return false;
}

string SageDynamicArrayType::to_string() {
    return str(array_type->to_string(), "[..]");
}

// array_type
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

bool SageArrayType::is_array() {return true;}
bool SageArrayType::is_pointer() {return false;}
bool SageArrayType::is_struct() {return false;}
bool SageArrayType::is_function() {return false;}

string SageArrayType::to_string() {
    return str(array_type->to_string(), "[", length, "]");
}

// reference_type
SageReferenceType::SageReferenceType(SageType *base_type) {
    pointer_type = TypeRegistery::get_pointer_type(base_type);

    // references are "fat pointers" to an array
    /*
     *   reference :: struct {
     *      array: void* // 8 bytes
     *      window_size: int // 8 bytes
     *   }
     */
    size = 16;
    alignment = 16;
}

SageReferenceType::SageReferenceType(SageType *base_type, int size): window_size(size) {
    pointer_type = TypeRegistery::get_pointer_type(base_type);
}

CanonicalType SageReferenceType::identify() {
    return REFERENCE;
}

bool SageReferenceType::match(SageType *other_type) {
    return other_type->identify() == REFERENCE && dynamic_cast<SageReferenceType *>(other_type)->pointer_type->match(pointer_type);
}

bool SageReferenceType::is_array() {
    return true;
}

bool SageReferenceType::is_pointer() {
    return true;
}

bool SageReferenceType::is_struct() {
    return false;
}

bool SageReferenceType::is_function() {
    return false;
}

string SageReferenceType::to_string() {
    SagePointerType *pointer_type_casted = dynamic_cast<SagePointerType *>(pointer_type);
    return str(pointer_type_casted->pointer_type->to_string(), "[]");
}

// function_type
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

bool SageFunctionType::is_array() {return false;}
bool SageFunctionType::is_pointer() {return false;}
bool SageFunctionType::is_struct() {return false;}
bool SageFunctionType::is_function() {return true;}

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

// struct_type
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

bool SageStructType::is_array() {return false;}
bool SageStructType::is_pointer() {return false;}
bool SageStructType::is_struct() {return true;}
bool SageStructType::is_function() {return false;}

string SageStructType::to_string() {
    return name;
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

SageType *TypeRegistery::get_pending_comptime_type() {
    auto key = std::make_pair(PENDING_COMPTIME, 1);
    auto it = builtin_types.find(key);
    if (it != builtin_types.end()) return it->second.get();

    auto type = std::make_unique<SageBuiltinType>(PENDING_COMPTIME, 1, 1);
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

SageType *TypeRegistery::get_dyn_array_type(SageType *element_type) {
    auto key = std::make_pair(element_type, 0);
    auto it = dyn_array_types.find(key);
    if (it != dyn_array_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageDynamicArrayType>(element_type, 0);
    dyn_array_types[key] = std::move(type);
    return dyn_array_types[key].get();
}

SageType *TypeRegistery::get_dyn_array_type(SageType *element_type, int length) {
    auto key = std::make_pair(element_type, length);
    auto it = dyn_array_types.find(key);
    if (it != dyn_array_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageDynamicArrayType>(element_type, length);
    dyn_array_types[key] = std::move(type);
    return dyn_array_types[key].get();
}

SageType *TypeRegistery::get_reference_type(SageType *base_type) {
    auto key = std::make_pair(base_type, 0);
    auto it = reference_types.find(key);
    if (it != reference_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageArrayType>(base_type, 0);
    reference_types[key] = std::move(type);
    return reference_types[key].get();
}

SageType *TypeRegistery::get_reference_type(SageType *base_type, int size) {
    auto key = std::make_pair(base_type, size);
    auto it = reference_types.find(key);
    if (it != reference_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageArrayType>(base_type, size);
    reference_types[key] = std::move(type);
    return reference_types[key].get();
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

SageType *TypeRegistery::get_function_type(std::vector<SageType *> parameter_types,
                                           std::vector<SageType *> return_types) {
    auto key = std::make_pair(return_types, parameter_types);
    auto it = function_types.find(key);
    if (it != function_types.end()) {
        return it->second.get();
    }

    auto type = std::make_unique<SageFunctionType>(return_types, parameter_types);
    function_types[key] = std::move(type);
    return function_types[key].get();
}


bool TypeRegistery::is_float64_type(SageType *type) {
    auto *float_type = get_builtin_type(FLOAT, 8);
    return type->match(float_type);
}

bool TypeRegistery::is_float32_type(SageType *type) {
    auto *float_type = get_builtin_type(FLOAT, 4);
    return type->match(float_type);
}

bool TypeRegistery::is_int64_type(SageType *type) {
    auto *int64_type = get_builtin_type(INT, 8);
    return type->match(int64_type);
}

bool TypeRegistery::is_int32_type(SageType *type) {
    auto *int32_type = get_builtin_type(INT, 4);
    return type->match(int32_type);
}

bool TypeRegistery::is_bool_type(SageType *type) {
    auto *bool_type = get_builtin_type(BOOL, 1);
    return type->match(bool_type);
}

bool TypeRegistery::is_char_type(SageType *type) {
    auto *char_type = get_builtin_type(CHAR, 1);
    return type->match(char_type);
}

bool TypeRegistery::is_null_type(SageType *type) {
    auto *null_type = get_builtin_type(VOID, 0);
    return type->match(null_type);
}








