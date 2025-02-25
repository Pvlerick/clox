#include "memory.h"
#include "compiler.h"
#include "object.h"
#include "vm.h"
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
  }

  if (newSize == 0) {
    free(pointer);
    return nullptr;
  }

  void *result = realloc(pointer, newSize);

  if (result == nullptr)
    exit(1);

  return result;
}

void markObject(Obj *obj) {
  debug("markObject\n");
  if (obj == nullptr)
    return;

  if (obj->isMarked)
    return;

  debug("checks done for %p\n", obj);
  if (obj->type == 3) {
    ObjString *str = (ObjString *)obj;
    debug("string is %s\n", str->isBorrowed ? "borrowed" : "owned");
    if (str->isBorrowed)
      debug("str location: %p\n", getCString(str));
  }
#ifdef DEBUG_LOG_GC
  debug("GC:  %p mark ", obj);
  printValue(OBJ_VAL(obj));
  debug("\n");
#endif
  debug("dbg print done\n");

  obj->isMarked = true;

  debug("marked\n");

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);

    vm.grayStack =
        (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);

    if (vm.grayStack == nullptr)
      exit(1);
  }

  vm.grayStack[vm.grayCount++] = obj;
  debug("done markObject\n");
}

void markValue(Value value) {
  debug("markValue\n");
  if (IS_OBJ(value))
    markObject(AS_OBJ(value));
  debug("done markValue\n");
}

static void markArray(ValueArray *array) {
  for (int i = 0; i > array->capacity; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *obj) {
#ifdef DEBUG_LOG_GC
  debug("GC:  %p blacken ", (void *)obj);
  printValue(OBJ_VAL(obj));
  debug("\n");
#endif

  switch (obj->type) {
  case OBJ_CLOSURE:
    ObjClosure *closure = (ObjClosure *)obj;
    markObject((Obj *)closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject((Obj *)closure->upvalues[i]);
    }
    break;
  case OBJ_FUNCTION:
    ObjFunction *fun = (ObjFunction *)obj;
    markObject((Obj *)fun->name);
    markArray(&fun->chunk.constants);
    break;
  case OBJ_UPVALUE:
    markValue(((ObjUpvalue *)obj)->closed);
    break;
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  }
}

static void freeObject(Obj *obj) {
  switch (obj->type) {
  case OBJ_CLOSURE:
    ObjClosure *closure = (ObjClosure *)obj;
    FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
    FREE(ObjClosure, obj);
    break;
  case OBJ_FUNCTION:
    ObjFunction *fun = (ObjFunction *)obj;
    freeChunk(&fun->chunk);
    FREE(ObjFunction, obj);
    break;
  case OBJ_NATIVE:
    FREE(ObjNative, obj);
    break;
  case OBJ_STRING:
    // TODO Check if that's correct - borrowed and owned
    FREE(ObjString, obj);
    break;
  case OBJ_UPVALUE:
    FREE(ObjUpvalue, obj);
    break;
  }
}

static void markRoots() {
  for (int i = 0; i < vm.stack.count; i++) {
    markValue(vm.stack.values[i]);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    // Both as.closure and as.function are pointers to Obj so any can be passed
    markObject((Obj *)vm.frames[i].as.closure);
  }

  for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != nullptr;
       upvalue = upvalue->next) {
    markObject((Obj *)upvalue);
  }

  markTable(&vm.globals);
  markCompilerRoots();
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj *obj = vm.grayStack[--vm.grayCount];
    blackenObject(obj);
  }
}

static void sweep() {
  Obj *previous = nullptr;
  Obj *obj = vm.objects;

  while (obj != nullptr) {
    debug("GC:  %p inspecting object for sweeping\n", obj);
    if (obj->isMarked) {
      debug("%p is marked\n", obj);
      obj->isMarked = false;
      previous = obj;
      obj = obj->next;
    } else {
      debug("GC:  %p is NOT marked\n", obj);
      debug("GC:  %p obj type: %d\n", obj, obj->type);
      Obj *unreached = obj;
      obj = obj->next;
      if (previous != nullptr) {
        previous->next = obj;
      } else {
        vm.objects = obj;
      }
      debug("GC:  %p is beeing freed\n");
      freeObject(unreached);
    }
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  debug("GC:  start\n");
#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

#ifdef DEBUG_LOG_GC
  debug("GC:  end\n");
#endif
}

void freeObjects() {
  Obj *obj = vm.objects;

  while (obj != nullptr) {
    Obj *next = obj->next;
    freeObject(obj);
    obj = next;
  }

  free(vm.grayStack);
}
