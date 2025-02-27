#include "memory.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif

    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
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
  if (obj == nullptr)
    return;

  if (IS_MARKED(obj))
    return;

#ifdef DEBUG_LOG_GC
  debug("GC:  %p mark '", obj);
  printValue(OBJ_VAL(obj));
  debug("'\n");
#endif

  MARK(obj);

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);

    vm.grayStack =
        (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);

    if (vm.grayStack == nullptr)
      exit(1);
  }

  vm.grayStack[vm.grayCount++] = obj;
}

void markValue(Value value) {
  if (IS_OBJ(value))
    markObject(AS_OBJ(value));
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *obj) {
#ifdef DEBUG_LOG_GC
  debug("GC:  %p blacken '", (void *)obj);
  printValue(OBJ_VAL(obj));
  debug("'\n");
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
    if (IS_MARKED(obj)) {
      previous = obj;
      obj = obj->next;
    } else {
#ifdef DEBUG_LOG_GC
      debug("GC:  %p is not marked and will be freed\n", obj);
      debug("GC:  freeing object: '");
      printObject(OBJ_VAL(obj));
      debug("' (type: %s)\n", getType(obj->type));
#endif
      Obj *unreached = obj;
      obj = obj->next;
      if (previous != nullptr) {
        previous->next = obj;
      } else {
        vm.objects = obj;
      }
      freeObject(unreached);
    }
  }
}

bool gcDisabled = false;

void collectGarbage() {
  if (gcDisabled) {
#ifdef DEBUG_LOG_GC
    debug("GC:  collection triggered but is currently disabled\n");
#endif
    return;
  }

#ifdef DEBUG_LOG_GC
  debug("GC:  start\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  debug("GC:  end\n");
  debug("GC:  collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif

  FLIP_MARK();
}

void disableGC() {
#ifdef DEBUG_LOG_GC
  debug("GC:  collection disabled\n");
#endif

  gcDisabled = true;
}

void enableGC() {
#ifdef DEBUG_LOG_GC
  debug("GC:  collection enabled\n");
#endif

  gcDisabled = false;
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
