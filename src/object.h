#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))

typedef enum {
  OBJ_STRING,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj *next;
};

// Note: this could be optimized using an union of FAM 
// union {
//   char *borrowed[] // Will have zero or one element
//   char owned[]     // Will have n + 1 elements if owned
// }
// which would get rid of the wasted space for *borrowed if not used
// This requires GCC 15 which I don't have :-)
struct ObjString {
  Obj obj;
  int length;
  uint32_t hash;
  bool isBorrowed;
  const char *borrowed;
  char owned[];
};

typedef struct StringRef {
  int length;
  const char *content;
} StringRef;

ObjString *allocateString(int length, int count, ...);
StringRef toStringRef(ObjString *string);
ObjString *borrowString(const char* chars, int length);
uint32_t hashString(const char *key, int length);
const char *getCString(ObjString *string);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return (IS_OBJ(value) && AS_OBJ(value)->type == type);
}

#endif
