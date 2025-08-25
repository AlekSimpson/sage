#include "../include/sage_types.h"

#include <memory>


SageBuiltinType::SageBuiltinType(CanonicalType type) {
    canonical_type = type;
}

CanonicalType SageBuiltinType::identify() {
    return canonical_type;
}

bool SageBuiltinType::match(SageType* other) {
    auto other_identity = other->identify();
    return other_identity == identify();
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

    for (int i = 0; i < parameter_types.size(); ++i) {
        if (!functype->parameter_types[i]->match(parameter_types[i])) {
            return false;
        }
    }

    for (int i = 0; i < return_type.size(); ++i) {
        if (!functype->return_type[i]->match(return_type[i])) {
            return false;
        }
    }

    return true;
}


SageValue::SageValue(int size, int _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.int_value = _value;
}

SageValue::SageValue(int size, float _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.float_value = _value;
}

SageValue::SageValue(int size, char _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.char_value = _value;
}

SageValue::SageValue(int size, bool _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.bool_value = _value;
}

SageValue::SageValue(int size, string* _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    // string* value_string = new string(_value);
    // value.string_value = value_string;
    value.string_value = _value;
}

SageValue::SageValue(int size, void* _value, SageType* valuetype) : bitsize(size), valuetype(valuetype) {
    value.complex_type = _value;
}

SageValue::SageValue() {}

SageValue::~SageValue() {
    //if (value.string_value != nullptr) {
    //    delete value.string_value;
    //}
}

uint64_t SageValue::load() {
    return 0; // TODO:
}

bool SageValue::is_null() {
    return false; // TODO: 
}

bool equals(const SageValue& other) {
    return false; // TODO:
}


/// Type Registery
SageType* TypeRegistery::get_builtin_type(CanonicalType canonical_type) {
    auto it = builtin_types.find(canonical_type);
    if (it != builtin_types.end()) {
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





