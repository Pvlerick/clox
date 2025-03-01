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
  UNMARK(obj);

  obj->next = vm.objects;
  vm.objects = obj;

#ifdef DEBUG_LOG_GC
  debug("GC:  %p allocate %zu bytes for %s\n", (void *)obj, size,
        getType(type));
#endif

  return obj;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  return klass;
}

ObjClosure *newClosure(ObjFunction *fun) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, fun->upvalueCount);
  for (int i = 0; i < fun->upvalueCount; i++) {
    upvalues[i] = nullptr;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = fun;
  closure->upvalues = upvalues;
  closure->upvalueCount = fun->upvalueCount;
  return closure;
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
  fun->upvalueCount = 0;
  fun->name = NULL;
  initChunk(&fun->chunk);
  return fun;
}

ObjInstance *newInstance(ObjClass *klass) {
  ObjInstance *inst = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  inst->klass = klass;
  initTable(&inst->fields);
  return inst;
}

ObjNative *newNative(NativeFn fun, int arity) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = fun;
  native->arity = arity;
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
    vm.objects = vm.objects->next;
    FREE(ObjString, string);
    return interned;
  }

  string->content[length] = '\0';
  string->length = length;
  string->isBorrowed = false;

  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  pop();

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
  string->hash = hash;
  memcpy((void *)string->content, (void *)&chars, sizeof(char *));

  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  pop();

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

ObjUpvalue *newUpvalue(int stackIndex) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->stackIndex = stackIndex;
  upvalue->next = nullptr;
  return upvalue;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_CLASS:
    ObjClass *klass = AS_CLASS(value);
    printf("%.*s", klass->name->length, getCString(klass->name));
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_INSTANCE:
    ObjInstance *inst = AS_INSTANCE(value);
    printf("%.*s instance", inst->klass->name->length,
           getCString(inst->klass->name));
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_STRING:
    ObjString *string = AS_STRING(value);
    printf("%.*s", string->length, getCString(string));
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  }
}
