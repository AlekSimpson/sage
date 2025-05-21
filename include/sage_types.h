#pragma once

enum SageType {
  BOOL,
  CHAR,
  INT,
  I8,
  I32, 
  I64,
  FLOAT,
  F32, 
  F64,
  VOID,
  ARRAY,
  POINTER
};

class SageValue {
public:
  virtual SageType get_type() = 0;
};

class SageIntValue {
public:
  int bitsize;
  int value;

  SageType get_type() override;
};

class SageCharValue {
public:
  char value;

  SageType get_type() override;
};

class SageArrayValue {
public:
  int headpointer; // points to a position in the stack or heap
  int size; // by default is -1 if size is unknown
  SageType nested_type;

  SageType get_type() override;
};
