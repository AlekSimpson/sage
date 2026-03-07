#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

using namespace std;

enum CanonicalType {
    BOOL,
    CHAR,
    VOID,
    INT,
    FLOAT,
    POINTER,
    ARRAY,
    REFERENCE,
    DYN_ARRAY,
    FUNC,
    CUSTOM,
    PENDING_COMPTIME
};

class SageValue;

class SageType {
public:
    int size; // bytes
    int alignment; // bytes

    virtual ~SageType() = default;
    virtual CanonicalType identify() = 0;
    virtual bool match(SageType *) = 0;
    virtual bool is_array() = 0;
    virtual bool is_pointer() = 0;
    virtual bool is_struct() = 0;
    virtual bool is_function() = 0;
    virtual string to_string() = 0;
    virtual string get_base_type_string() = 0;
    virtual SageValue get_default_value() = 0;
    virtual bool is_callable() = 0;
};

class SageBuiltinType : public SageType {
public:
    CanonicalType canonical_type;

    SageBuiltinType(CanonicalType, int size, int alignment);

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SagePointerType : public SageType {
public:
    SageType *pointer_type;

    SagePointerType(SageType *);

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SageArrayType : public SageType {
public:
    SageType *array_type;
    int length;

    SageArrayType(SageType *, int);

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SageFunctionType : public SageType {
public:
    vector<SageType *> return_type;
    vector<SageType *> parameter_types;

    SageFunctionType(
        vector<SageType *> parameters,
        vector<SageType *> return_types
    );

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SageStructType : public SageType {
public:
    string name;
    vector<SageType *> member_types;

    SageStructType(
        string name,
        vector<SageType *> member_types,
        int size,
        int alignment
    );

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SageDynamicArrayType : public SageType {
public:
    SageType *array_type;
    int length = 0; // current max length
    int capacity = 15; // current amount of slots used up

    SageDynamicArrayType(SageType * basetype, int length);
    SageDynamicArrayType(SageType * basetype);

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class SageReferenceType : public SageType {
public:
    SageType *pointer_type;
    int window_size = 0;

    SageReferenceType(SageType *);
    SageReferenceType(SageType *, int window_size);

    CanonicalType identify() override;
    bool match(SageType *) override;
    bool is_array() override;
    bool is_pointer() override;
    bool is_struct() override;
    bool is_function() override;
    string to_string() override;
    string get_base_type_string() override;
    SageValue get_default_value() override;
    bool is_callable() override;
};

class TypeRegistery {
private:
    static std::unordered_map<std::pair<CanonicalType, int>, std::unique_ptr<SageType> > builtin_types;
    static std::unordered_map<SageType *, std::unique_ptr<SageType> > pointer_types;
    static std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > array_types;
    static std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > dyn_array_types;
    static std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > reference_types;
    static std::unordered_map<string, std::unique_ptr<SageType> > struct_types;
    static std::unordered_map<std::pair<std::vector<SageType *>, std::vector<SageType *> >, std::unique_ptr<SageType> >
    function_types;

public:
    static SageType *get_pending_comptime_type();
    static SageType *get_builtin_type(CanonicalType canonical_type, int bytesize);
    static SageType *get_byte_type(CanonicalType canonical_type);
    static SageType *get_float_type(int size);
    static SageType *get_integer_type(int size);
    static SageType *get_pointer_type(SageType *base_type);
    static SageType *get_dyn_array_type(SageType *element_type);
    static SageType *get_dyn_array_type(SageType *element_type, int starting_element_size); // initializes dyn array type with 1.7x mem based on starting_element_size
    static SageType *get_array_type(SageType *element_type, int size);
    static SageType *get_reference_type(SageType *base_type, int size);
    static SageType *get_function_type(std::vector<SageType *> parameter_tyeps, std::vector<SageType *> function_types);


    static SageType *get_struct_type(string name, std::vector<SageType *> member_types);

    static bool is_builtin_primitive(SageType *type);
    static bool is_float64_type(SageType *type);
    static bool is_float32_type(SageType *type);
    static bool is_int64_type(SageType *type);
    static bool is_int32_type(SageType *type);
    static bool is_bool_type(SageType *type);
    static bool is_char_type(SageType *type);
    static bool is_null_type(SageType *type);
};

namespace std {
    // Hash function for std::pair<CanonicalType, int>
    template<>
    struct hash<std::pair<CanonicalType, int> > {
        size_t operator()(const std::pair<CanonicalType, int> &p) const {
            size_t h1 = std::hash<int>{}(static_cast<int>(p.first));
            size_t h2 = std::hash<int>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };

    // Hash function for std::pair<SageType*, int>
    template<>
    struct hash<std::pair<SageType *, int> > {
        size_t operator()(const std::pair<SageType *, int> &p) const {
            size_t h1 = std::hash<SageType *>{}(p.first);
            size_t h2 = std::hash<int>{}(p.second);
            return h1 ^ (h2 << 1); // Combine the two hash values
        }
    };

    // Hash function for std::vector<SageType*>
    template<>
    struct hash<std::vector<SageType *> > {
        size_t operator()(const std::vector<SageType *> &v) const {
            size_t seed = v.size();
            for (auto &ptr: v) {
                seed ^= std::hash<SageType *>{}(ptr) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    // Hash function for std::pair<std::vector<SageType*>, std::vector<SageType*>>
    template<>
    struct hash<std::pair<std::vector<SageType *>, std::vector<SageType *> > > {
        size_t operator()(const std::pair<std::vector<SageType *>, std::vector<SageType *> > &p) const {
            size_t h1 = std::hash<std::vector<SageType *> >{}(p.first);
            size_t h2 = std::hash<std::vector<SageType *> >{}(p.second);
            return h1 ^ (h2 << 1); // Combine the two hash values
        }
    };
}
