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

// Note: will allocate length + 1 to allow for \0
ObjString *allocateString(int length) {
  ObjString *string = (ObjString *)allocateObject(
      sizeof(ObjString) + sizeof(char) * (length + 1), OBJ_STRING);

  string->length = length;

  return string;
}

ObjString *copyString(const char *chars, int length) {
  ObjString *string = allocateString(length);

  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';

  return string;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}
