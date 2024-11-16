#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include <stdint.h>

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void *memRealloc(void *pointer, size_t newSize);
void* memAlloc(size_t size);
void memFree(void* pointer);

#ifndef NO_MANUAL_MEMORY_MANAGEMENT

typedef struct HeapBlock HeapBlock;
struct HeapBlock {
  size_t size;
  bool isFree;
  void* content; 
  struct HeapBlock* previous;
  struct HeapBlock* next;
};

typedef struct {
  HeapBlock* first;
} Heap;

void dumpHeap();
void dumpHeapBlock(HeapBlock* block);
void checkHeapIntegrity();

#endif

#endif
