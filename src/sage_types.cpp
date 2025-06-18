#include "../include/sage_types.h"



SageBuiltinType::SageBuiltinType(CanonicalType type) {
    canonical_type = type;
}

CanonicalType SageBuiltinType::identify() {
    return canonical_type;
}

bool SageBuiltinType::match(SageType other) {
    auto other_identity = other.identify();
    return other_identity == identify();
}


SagePointerType::SagePointerType(SageType pointee) : pointer_type(pointee) {}
CanonicalType SagePointerType::identify() {
    return POINTER;
}

bool SagePointerType::match(SageType other) {
    if (other.identify() != POINTER) {
        return false;
    }

    SagePointerType other_pointer = dynamic_cast<SagePointerType>(other);
    return pointer_type.match(other_pointer.pointer_type);
}


SageArrayType::SageArrayType(SageType element_type, int size) : array_type(element_type), size(size) {}
CanonicalType SageArrayType::SageArrayType() {
    return ARRAY;
}

bool SageArrayType::match(SageType other) {
    if (other.identify() != ARRAY) {
        return false;
    }

    SageArrayType other_array = dynamic_cast<SageArrayType>(other);
    return (size != other_array.size) && array_type.match(other_array.array_type);
}

SageFunctionType::SageFunctionType(vector<SageType> returns, vector<SageType> params) :
    return_type(returns), parameter_types(params) {}

CanonicalType SageFunctionType::identify() {
    return FUNCTION;
}

bool SageFunctionType::match(SageType other) {
    if (other.identify() != FUNCTION) {
        return false;
    }
    
    SageFunctionType functype = dynamic_cast<SageFunctionType>(other);
    if (functype.parameter_types.size() != parameter_types.size() || functype.return_type.size() != return_type.size()) {
        return false;
    }

    for (int i = 0; i < parameter_types.size(); ++i) {
        if (!functype.parameter_types[i].match(parameter_types[i])) {
            return false;
        }
    }

    for (int i = 0; i < return_type.size(); ++i) {
        if (!functype.return_type[i].match(return_type[i])) {
            return false;
        }
    }

    return true;
}
