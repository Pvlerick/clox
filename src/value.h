#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <stdlib.h>
#include <err.h>
#include <stdio.h>
#include <string.h>

typedef struct Obj Obj;
typedef struct ObjFunction ObjFunction;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
//#define QNAN 18446744073709027328##U
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL          1 // 001
#define TAG_FALSE        2 // 010
#define TAG_TRUE         3 // 011
#define TAG_SHORT_STRING 4 // 100

typedef uint64_t Value;

#define NUMBER_VAL(num) numToValue(num)
#define AS_NUMBER(value) valueToNum(value)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)

#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)

#define SHORT_STRING_VAL(start, length) charToStr(start, length)
#define AS_SHORT_STRING(value) ((char *)(&value) + 1)
#define IS_SHORT_STRING(value) ((value) & QNAN | TAG_SHORT_STRING == (QNAN | TAG_SHORT_STRING))

#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define IS_NIL(value) ((value) == NIL_VAL)

#define OBJ_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
#define AS_OBJ(value) ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))
#define IS_OBJ(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

static inline double valueToNum(Value value) {
  // return *(double*)&value;
  double num;
  memcpy(&num, &value, sizeof(double));
  return num;
}

static inline Value numToValue(double num) {
  // return *(Value*)&num;
  Value value;
  memcpy(&value, &num, sizeof(Value));
  return value;
}

static Value charToStr(const char *start, int length) {
  if (length > 4)
    err(EXIT_FAILURE, "Cannot call charToStr with a string longer than 4\n");

  Value val = 0 | QNAN | TAG_SHORT_STRING;
  char *ptr = ((char *)&val) + 1;

  memcpy(ptr, start, length);

  ptr[length] = '\0';

  return val;
}


#else

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_SHORT_STRING,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    char str[5];
    Obj *obj;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_SHORT_STRING(value) ((value).type == VAL_SHORT_STRING)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_SHORT_STRING(value) ((value).as.str)
#define AS_OBJ(value) ((value).as.obj)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define SHORT_STRING_VAL(start, length) valueToShortString(start, length)
#define OBJ_VAL(value) ((Value){VAL_OBJ, {.obj = (Obj*)value}})

static inline Value valueToShortString(const char* start, int length) {
  Value val = {.type = VAL_SHORT_STRING};
  memcpy(val.as.str, start, length);
  val.as.str[length] = '\0';
  return val;
}

#endif

typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void valueArrayDump(ValueArray* array);

void printValue(Value value);

#endif
