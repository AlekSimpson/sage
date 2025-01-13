#include <stdbool.h>
#include <stdlib.h>
#include "include/stack.h"

StackResult pop(Stack* stack) {
    StackResult retval;
    if (stack->count == 0) {
        retval.success = false;
        return retval;
    }

    // we will pretend that popped elements are "deleted" by shifting our index backwards
    // this will mean that when push is called we will overwrite the "deleted" data which is fine because its been "deleted"
    stack->count--;
    retval.element = stack->elements[stack->count];
    retval.success = true;
    return retval;
}

void push(Stack* stack, StackElement element) {
    if (stack->count == stack->capacity) {
        // allocate double the current capacity
        int new_capacity = 2 * stack->capacity;
        StackElement* new_storage = (StackElement*)malloc(sizeof(StackElement)*new_capacity);

        // copy over the current elements
        for (int i = 0; i < stack->count-1; ++i) {
            new_storage[i] = stack->elements[i];
        }

        // free the old array
        free(stack->elements);

        // update the capacity to reflect new total capacity
        stack->capacity = new_capacity;
    }

    stack->elements[stack->count] = element;
    stack->count++;
}

bool empty(Stack* stack) {
    return (stack->count == 0);
}

Stack* init_new_stack(int initial_count) {
    Stack* stack = (Stack*)malloc(sizeof(Stack));
    StackElement* elements = (StackElement*)malloc(sizeof(StackElement) * (initial_count + 10));
    stack->elements = elements;
    stack->capacity = initial_count + 10;
    stack->count = 0;
    return stack;
}

void deinit_stack(Stack* stack) {
    free(stack->elements);
    free(stack);
}


