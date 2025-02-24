#include "stack.h"
#include "memory.h"

void initStack(Stack *stack) {
  stack->count = 0;
  stack->capacity = 0;
  stack->values = nullptr;
  stack->top = nullptr;
}

void freeStack(Stack *stack) {
  FREE_ARRAY(Value, stack->values, stack->capacity);
  initStack(stack);
}

void pushOnStack(Stack *stack, Value value) {
  if (stack->capacity < stack->count + 1) {
    if (IS_OBJ(value))
      disableGC();

    int oldCapacity = stack->capacity;
    stack->capacity = GROW_CAPACITY(oldCapacity);
    stack->values =
        GROW_ARRAY(Value, stack->values, oldCapacity, stack->capacity);
    stack->top = stack->values + stack->count;

    if (IS_OBJ(value))
      enableGC();
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

Value peekFromStack(Stack *stack, int distance) {
  return stack->top[-1 - distance];
}

Value stackGet(Stack *stack, int index) { return stack->values[index]; }

void stackSet(Stack *stack, int index, Value value) {
  stack->values[index] = value;
}

void stackReset(Stack *stack, int index) {
  stack->count = index;
  stack->top = &stack->values[stack->count];
}

void stackDrop(Stack *stack, int count) {
  stack->top -= count;
  stack->count -= count;
}
