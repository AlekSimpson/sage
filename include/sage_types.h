#pragma once

#include <stdlib.h>
#include <vector>
#include <unordered_map>  // For std::unordered_map
#include <memory>         // For std::unique_ptr  
#include <string>         // For std::string
#include <utility>        // For std::pair

using namespace std;

enum CanonicalType {
  BOOL,
  CHAR,
  I8,
  I32, 
  I64,
  F32, 
  F64,
  VOID,
  POINTER,
  ARRAY,
  FUNC,
};

class SageType {
public:
  virtual ~SageType() = default;
  virtual CanonicalType identify() = 0;
  virtual bool match(SageType*) = 0;
};

class SageBuiltinType : public SageType {
public:
  CanonicalType canonical_type;
  SageBuiltinType(CanonicalType);
  CanonicalType identify() override;
  bool match(SageType*) override;
};

class SagePointerType : public SageType {
public:
  SageType* pointer_type;
  SagePointerType(SageType*);
  CanonicalType identify() override;
  bool match(SageType*) override;
};

class SageArrayType : public SageType {
public:
  SageType* array_type;
  int size;
  SageArrayType(SageType*, int);
  CanonicalType identify() override;
  bool match(SageType*) override;
};

class SageFunctionType : public SageType {
public:
  vector<SageType*> return_type;
  vector<SageType*> parameter_types;
  SageFunctionType(
    vector<SageType*> returntype, 
    vector<SageType*> parameters
  );
  CanonicalType identify() override;
  bool match(SageType*) override;
};

union value_t {
  int int_value; // TODO: this also needds to have the other bit size types also
  float float_value;
  char char_value;
  bool bool_value;
  string* string_value;
  void* complex_type;
};

class SageValue {
public:
  int bitsize;
  value_t value;
  SageType* valuetype;

  SageValue();
  SageValue(int, int, SageType*);
  SageValue(int, float, SageType*);
  SageValue(int, char, SageType*);
  SageValue(int, bool, SageType*);
  SageValue(int, string*, SageType*);
  SageValue(int, void*, SageType*);
  ~SageValue();
  uint64_t load();
  bool is_null();
  bool equals(const SageValue& other);
};

class TypeRegistery {
private:
  static std::unordered_map<CanonicalType, std::unique_ptr<SageType>> builtin_types;
  static std::unordered_map<SageType*, std::unique_ptr<SageType>> pointer_types;
  static std::unordered_map<std::pair<SageType*, int>, std::unique_ptr<SageType>> array_types;
  static std::unordered_map<std::pair<std::vector<SageType*>, std::vector<SageType*>>, std::unique_ptr<SageType>> function_types;

public:
  static SageType* get_builtin_type(CanonicalType canonical_type);
  static SageType* get_pointer_type(SageType* base_type);
  static SageType* get_array_type(SageType* element_type, int size);
  static SageType* get_function_type(std::vector<SageType*> return_types, std::vector<SageType*> parameter_types);
};

namespace std {
    // Hash function for std::pair<SageType*, int>
    template<>
    struct hash<std::pair<SageType*, int>> {
        size_t operator()(const std::pair<SageType*, int>& p) const {
            size_t h1 = std::hash<SageType*>{}(p.first);
            size_t h2 = std::hash<int>{}(p.second);
            return h1 ^ (h2 << 1);  // Combine the two hash values
        }
    };
    
    // Hash function for std::vector<SageType*>
    template<>
    struct hash<std::vector<SageType*>> {
        size_t operator()(const std::vector<SageType*>& v) const {
            size_t seed = v.size();
            for (auto& ptr : v) {
                seed ^= std::hash<SageType*>{}(ptr) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
    
    // Hash function for std::pair<std::vector<SageType*>, std::vector<SageType*>>
    template<>
    struct hash<std::pair<std::vector<SageType*>, std::vector<SageType*>>> {
        size_t operator()(const std::pair<std::vector<SageType*>, std::vector<SageType*>>& p) const {
            size_t h1 = std::hash<std::vector<SageType*>>{}(p.first);
            size_t h2 = std::hash<std::vector<SageType*>>{}(p.second);
            return h1 ^ (h2 << 1);  // Combine the two hash values
        }
    };
}
