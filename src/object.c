#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdarg.h>
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

uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

StringRef toStringRef(ObjString *string) {
  return (StringRef){.length = string->length, .content = getCString(string)};
}

ObjString *allocateString(int length, int count, ...) {
  ObjString *string = (ObjString *)allocateObject(
      sizeof(ObjString) + sizeof(char) * (length + 1), OBJ_STRING);

  va_list refs;
  va_start(refs, count);

  int offset = 0;

  for (int i = 0; i < count; i++) {
    StringRef ref = va_arg(refs, StringRef);
    memcpy(string->owned + offset, ref.content, ref.length);
    offset += ref.length;
  }

  va_end(refs);

  string->hash = hashString(string->owned, length);

  ObjString *interned =
      tableFindString(&vm.strings, string->owned, length, string->hash);

  if (interned != nullptr) {
    FREE(ObjString, string);
    return interned;
  }

  string->owned[length] = '\0';
  string->length = length;
  string->isBorrowed = false;

  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

ObjString *borrowString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != nullptr)
    return interned;

  ObjString *string =
      (ObjString *)allocateObject(sizeof(ObjString), OBJ_STRING);

  string->length = length;
  string->isBorrowed = true;
  string->borrowed = chars;
  string->hash = hashString(chars, length);

  tableSet(&vm.strings, string, NIL_VAL);

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
