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
Value peekFromStack(Stack* stack, int distance);
Value stackGet(Stack *stack, int index);
void stackSet(Stack *stack, int index, Value value);
void stackReset(Stack *stack, int index);
void stackDrop(Stack *stack, int count);

#endif
