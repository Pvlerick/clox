#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objType)                                            \
  (type *)allocateObject(sizeof(type), objType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *obj = (Obj *)reallocate(nullptr, 0, size);
  obj->type = type;

  obj->next = vm.objects;
  vm.objects = obj;

  return obj;
}

ObjString *allocateString(int length) {
  ObjString *string = (ObjString *)allocateObject(
      sizeof(ObjString) + sizeof(char) * (length + 1), OBJ_STRING);

  string->length = length;
  string->isBorrowed = false;

  return string;
}

ObjString *borrowString(const char *chars, int length) {
  ObjString *string =
      (ObjString *)allocateObject(sizeof(ObjString), OBJ_STRING);

  string->length = length;
  string->isBorrowed = true;
  string->borrowed = chars;

  return string;
}

const char *getCString(ObjString *string) {
  return string->isBorrowed ? string->borrowed : string->owned;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    ObjString *string = AS_STRING(value);
    printf("%.*s", string->length, getCString(string));
    break;
  }
}
