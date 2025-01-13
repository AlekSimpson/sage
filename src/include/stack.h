#ifndef STACK
#define STACK

#include "token.h"
#include <stdbool.h>

typedef union {
    Token token;
    int integer;
    char character;
} StackElement;

typedef struct {
    StackElement element;
    bool success;
} StackResult;

typedef struct {
    StackElement* elements;
    int capacity;
    int count;
} Stack;

StackResult pop(Stack* stack);
void push(Stack* stack, StackElement element);
bool empty(Stack* stack);
Stack* init_new_stack(int initial_count);
void deinit_stack(Stack* stack);

#endif
