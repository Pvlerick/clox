#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->values = NULL;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapactiy = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapactiy);
    array->values =
        GROW_ARRAY(typeof(Value), array->values, oldCapactiy, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void printValue(Value value) { printf("%g", value); }
