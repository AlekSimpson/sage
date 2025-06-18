#pragma once

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
  virtual CanonicalType identify() = 0;
  virtual bool match(SageType) = 0;
};

class SageBuiltinType : public SageType {
public:
  CanonicalType canonical_type;
  SageBuiltinType(CanonicalType);
  CanonicalType identify() override;
  bool match(SageType) override;
};

class SagePointerType : public SageType {
public:
  SageType pointer_type;
  SagePointerType(SageType);
  CanonicalType identify() override;
  bool match(SageType) override;
};

class SageArrayType : public SageType {
public:
  SageType array_type;
  int size;
  SageArrayType(SageType, int);
  CanonicalType identify() override;
  bool match(SageType) override;
};

class SageFunctionType : public SageType {
public:
  vector<SageType> return_type;
  vector<SageType> parameter_types;
  SageFunctionType(
    vector<SageType> returntype, 
    vector<SageType> parameters
  );
  CanonicalType identify() override;
  bool match(SageType) override;
};

union value_t {
  int int_value;
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
  SageType valuetype;

  SageValue(int, value_t, SageType);
};

