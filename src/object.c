#include "object.h"
#include "common.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

ObjString *newOwnedString(const char *start, size_t length) {
  ObjString *string =
      (ObjString *)allocateObject(sizeof(ObjString) + length, OBJ_STRING);

  string->length = length;
  string->isBorrowed = false;
  string->hash = hashString(start, length);
  memcpy((void *)string->content, (void *)start, length);

  return string;
}

ObjFunction *newFunction() {
  ObjFunction *fun = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  fun->arity = 0;
  fun->name = NULL;
  initChunk(&fun->chunk);
  return fun;
}

ObjNative *newNative(NativeFn fun) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = fun;
  return native;
}

static void printFunction(ObjFunction *fun) {
  if (fun->name == nullptr)
    printf("<script>");
  else
    printf("<fn %.*s>", fun->name->length, getCString(fun->name));
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
    memcpy(string->content + offset, ref.content, ref.length);
    offset += ref.length;
  }

  va_end(refs);

  string->hash = hashString(string->content, length);

  ObjString *interned =
      tableFindString(&vm.strings, string->content, length, string->hash);

  if (interned != nullptr) {
    FREE(ObjString, string);
    return interned;
  }

  string->content[length] = '\0';
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

  ObjString *string = (ObjString *)allocateObject(
      sizeof(ObjString) + sizeof(char *), OBJ_STRING);

  string->length = length;
  string->isBorrowed = true;
  string->hash = hashString(chars, length);
  memcpy((void *)string->content, (void *)&chars, sizeof(char *));

  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

const char *getCString(ObjString *string) {
  return string->isBorrowed ? *(char **)string->content : string->content;
}

const char *copyString(ObjString *string) {
  char *loc = (char *)malloc(string->length + 1);
  memcpy(loc, getCString(string), string->length);
  loc[string->length] = '\0';
  return loc;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_STRING:
    ObjString *string = AS_STRING(value);
    printf("%.*s", string->length, getCString(string));
    break;
  }
}
