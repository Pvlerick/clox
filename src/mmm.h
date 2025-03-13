#ifndef clox_mmm_h
#define clox_mmm_h

#include "common.h"

void dumpHeap();
void checkHeapIntegrity();
void *__wrap_malloc(size_t size);
void __wrap_free(void *ptr);
void *__wrap_realloc(void *ptr, size_t size);

#endif
