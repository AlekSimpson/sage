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
};

class SageType {};

class SageBuiltinType : public SageType {
public:
  CanonicalType canonical_type;
  SageBuiltinType(CanonicalType);
};

class SagePointerType : public SageType {
public:
  SageType pointer_type;
  SagePointerType(SageType);
};

class SageArrayType : public SageType {
public:
  SageType array_type;
  int size;
  SageArrayType(SageType, int);
};

class SageFunctionType : public SageType {
public:
  SageType return_type;
  vector<SageType> parameter_types;
  SageFunctionType(SageType returntype, vector<SageType> parameters);
};


class SageValue {
public:
  virtual int get_value() = 0;
  virtual SageType get_type() = 0;
};

class SageIntValue : public SageValue {
public:
  int bitsize;
  int value;

  SageIntValue(int value, int bitsize);

  SageType get_type() override;
  int get_value() override;
};

class SageFloatValue : public SageValue {
public:
  int bitsize;
  float value;

  SageFloatValue(float value, int bitsize);

  SageType get_type() override;
  int get_value() override;
  float get_float_value();
};

class SageCharValue : public SageValue {
public:
  char value;

  SageType get_type() override;
  int get_value() override;
};

class SageArrayValue : public SageValue {
public:
  int headpointer; // points to a position in the stack or heap
  int size; // by default is -1 if size is unknown
  SageType nested_type;

  SageType get_type() override;
  int get_value() override;
};
