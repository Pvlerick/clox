#include "stack.h"
#include "memory.h"
#include <stdio.h>

void initStack(Stack *stack) {
  stack->count = 0;
  stack->capacity = 0;
  stack->values = NULL;
  stack->top = NULL;
}

void freeStack(Stack *stack) {
  FREE_ARRAY(Value, stack->values, stack->capacity);
  initStack(stack);
}

void pushOnStack(Stack *stack, Value value) {
  if (stack->capacity < stack->count + 1) {
    int oldCapacity = stack->capacity;
    stack->capacity = GROW_CAPACITY(oldCapacity);
    stack->values =
        GROW_ARRAY(Value, stack->values, oldCapacity, stack->capacity);
    stack->top = stack->values + stack->count;
  }

  stack->top = &stack->values[stack->count];
  *stack->top = value;

  stack->top++;
  stack->count++;
}

Value popFromStack(Stack *stack) {
  stack->top--;
  stack->count--;
  return *stack->top;
}
