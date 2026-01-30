#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
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
    virtual string to_string() = 0;
    // TODO: virtual SageValue get_default_value() = 0;
};

class SageBuiltinType : public SageType {
public:
    CanonicalType canonical_type;

    SageBuiltinType(CanonicalType, int size, int alignment);

    CanonicalType identify() override;
    bool match(SageType *) override;
    string to_string() override;
};

class SagePointerType : public SageType {
public:
    SageType *pointer_type;

    SagePointerType(SageType *);

    CanonicalType identify() override;
    bool match(SageType *) override;
    string to_string() override;
};

class SageArrayType : public SageType {
public:
    SageType *array_type;
    int length;

    SageArrayType(SageType *, int);

    CanonicalType identify() override;
    bool match(SageType *) override;
    string to_string() override;
};

class SageFunctionType : public SageType {
public:
    vector<SageType *> return_type;
    vector<SageType *> parameter_types;

    SageFunctionType(
        vector<SageType *> returntype,
        vector<SageType *> parameters
    );

    CanonicalType identify() override;
    bool match(SageType *) override;
    string to_string() override;
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
    string to_string() override;
};

union primitive_union {
    int int_value; // TODO: this also needs to have the other bit size types also
    float float_value;
    char char_value;
    bool bool_value;
    void *complex_value;
};

class TypeRegistery {
private:
    static std::unordered_map<std::pair<CanonicalType, int>, std::unique_ptr<SageType> > builtin_types;
    static std::unordered_map<SageType *, std::unique_ptr<SageType> > pointer_types;
    static std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > array_types;
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
    static SageType *get_array_type(SageType *element_type, int size);
    static SageType *get_function_type(std::vector<SageType *> return_types, std::vector<SageType *> parameter_types);
    static SageType *get_struct_type(string name, std::vector<SageType *> return_types);
};

class SageValue {
public:
    primitive_union value;
    SageType *valuetype = TypeRegistery::get_byte_type(VOID);
    bool nullvalue = true;

    SageValue();
    SageValue(int, SageType *);
    SageValue(float, SageType *);
    SageValue(char, SageType *);
    SageValue(bool, SageType *);
    SageValue(void *, SageType *);
    SageValue(uint64_t register_value);
    SageValue(int);
    SageValue(float);
    ~SageValue();

    uint64_t load();

    bool is_null();

    bool equals(const SageValue &other);

    operator uint64_t() { return load(); }

    // For instruction operands
    int as_operand() const;

    // Convenience getters with automatic conversion
    int32_t as_i32() const { return value.int_value; }
    float as_float() const { return value.float_value; }
    void *as_ptr() const { return value.complex_value; }
    // DEPRECATED: TODO: FIX: const char *as_charbuff() const { return static_cast<const char *>(value.complex_value); }
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
