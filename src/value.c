#include <err.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->values = nullptr;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapactiy = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapactiy);
    array->values =
        GROW_ARRAY(Value, array->values, oldCapactiy, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void valueArrayDump(ValueArray *array) {
  printf("dumping varlue array (count: %d, capacity: %d)\n", array->count,
         array->capacity);
  for (int i = 0; i < array->capacity; i++) {
    Value *value = &array->values[i];
    if (value != nullptr) {
      printf("[%d | ", i);
      if (i < array->count) {
        printf("'");
        printValue(*value);
        printf("' ]\n");
      } else {
        printf("(empty) ]\n");
      }
    }
  }
}

#ifdef NAN_BOXING
static void asBinary(uint64_t number, char binary[65]) {
  for (int i = 63; i >= 0; i--)
    binary[63 - i] = (number & (1ULL << i)) ? '1' : '0';
  binary[64] = '\0';
}
#endif

void printValue(Value value) {
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    printf("%s", AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    printf("%g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(value);
  } else {
    char binary[65];
    asBinary(value, binary);
    err(64, "Unhandeled value type: '%s'", binary);
  }
#else
  switch (value.type) {
  case VAL_BOOL:
    printf("%s", AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_NUMBER:
    printf("%g", AS_NUMBER(value));
    break;
  case VAL_SHORT_STRING:
    printf("%s", AS_SHORT_STRING(value));
    break;
  case VAL_OBJ:
    printObject(value);
    break;
  default:
    err(64, "Unhandeled value type '%d'", value.type);
    break;
  }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
#else
  if (a.type != b.type)
    return false;

  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_SHORT_STRING:
    return strncmp(a.as.str, b.as.str, 4) == 0;
  case VAL_OBJ:
    return AS_OBJ(a) == AS_OBJ(b);
  default:
    return false;
  }
#endif
}
