#ifndef clox_stack_h
#define clox_stack_h

#include "common.h"
#include "value.h"

typedef struct {
  int count;
  int capacity;
  Value* values;
  Value* top;
} Stack;

void initStack(Stack* stack);
void freeStack(Stack* stack);
void pushOnStack(Stack* stack, Value value);
Value popFromStack(Stack* stack);

#endif
