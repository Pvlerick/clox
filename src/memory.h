#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>

#define ALLOCATE(type, count) \
  (type*)reallocate(nullptr, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

#define MARK(obj) ((obj)->mark = vm.markValue)
#define UNMARK(obj) ((obj)->mark = !vm.markValue)
#define IS_MARKED(obj) ((obj)->mark == vm.markValue)
#define FLIP_MARK() (vm.markValue = !vm.markValue)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void markObject(Obj *obj);
void markValue(Value value);
void collectGarbage();
void disableGC();
void enableGC();
void freeObjects();

#endif

