#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))

#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value)))

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))

typedef enum {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
} ObjType;

struct Obj {
  ObjType type;
  bool mark;
  struct Obj *next;
};

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  int arity;
  NativeFn function;
} ObjNative;

struct ObjFunction {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString *name;
};

// Note: in the case of a borrowed string, the FAM is reinterpreted as a char*
// This would be better with a union, but that's an extension of GCC that's not in the mainline yet
struct ObjString {
  Obj obj;
  int length;
  uint32_t hash;
  bool isBorrowed;
  char content[];
};

typedef struct ObjUpvalue {
  Obj obj;
  int stackIndex;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct StringRef {
  int length;
  const char *content;
} StringRef;

ObjString *newOwnedString(const char *start, size_t length);
ObjClosure *newClosure(ObjFunction *fun);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn fun, int arity);
ObjString *allocateString(int length, int count, ...);
StringRef toStringRef(ObjString *string);
ObjString *borrowString(const char* chars, int length);
uint32_t hashString(const char *key, int length);
const char *getCString(ObjString *string);
const char *copyString(ObjString *string);
void debugString(ObjString *string);
ObjUpvalue *newUpvalue(int stackIndex);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return (IS_OBJ(value) && AS_OBJ(value)->type == type);
}

#ifdef DEBUG_LOG_GC
static inline const char *getType(ObjType type) {
  switch (type) {
  case OBJ_FUNCTION:
    return "function";
  case OBJ_STRING:
    return "string";
  case OBJ_NATIVE:
    return "native function";
  case OBJ_CLOSURE:
    return "closure";
  case OBJ_UPVALUE:
    return "upvalue";
  }
}
#endif

#endif
