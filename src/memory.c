#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANUAL_MEMORY_MANAGEMENT 1
#ifndef MANUAL_MEMORY_MANAGEMENT

void *memRealloc(void *pointer, size_t newSize) {
  return realloc(pointer, newSize);
}

void memFree(void *pointer) { free(pointer); }

#else

const int HEAP_SIZE = 1 << 16;
Heap heap = {.first = NULL};

void initHeap() {
  printf("MEM: heap block size: %lu\n", sizeof(HeapBlock));
  printf("MEM: allocating %d bytes for heap\n", HEAP_SIZE);

  void *heapStart = malloc(HEAP_SIZE);

  HeapBlock *first = heapStart;
  first->size = HEAP_SIZE - sizeof(HeapBlock);
  first->isFree = true;
  first->content = heapStart + sizeof(HeapBlock);
  first->previous = NULL;
  first->next = NULL;

  heap.first = first;
}

void *memAlloc(size_t size) {
  if (heap.first == NULL)
    initHeap();

  printf("MEM: allocating %zu bytes\n", size);

  HeapBlock *firstSuitable = heap.first;
  while (!(firstSuitable->isFree &&
           ((firstSuitable->size == size) ||
            firstSuitable->size > size + sizeof(HeapBlock))))
    firstSuitable = firstSuitable->next;

  if (firstSuitable->size == size) {
    // Requested size perfectly match free size, just update the block
    firstSuitable->isFree = false;
    return firstSuitable->content;
  }

  HeapBlock *next = firstSuitable->content + size;
  next->size = firstSuitable->size - (sizeof(HeapBlock) + size);
  next->isFree = true;
  next->content = firstSuitable->content + sizeof(HeapBlock) + size;
  next->previous = firstSuitable;
  next->next = firstSuitable->next;
  if (firstSuitable->next != NULL)
    firstSuitable->next->previous = next;

  firstSuitable->size = size;
  firstSuitable->isFree = false;
  firstSuitable->next = next;

  return firstSuitable->content;
}

void memFree(void *pointer) {
  if (heap.first == NULL) {
    fprintf(stderr, "Error: trying to free unallocated pointer\n");
    exit(1);
  }

  HeapBlock *current = heap.first;

  while ((current != NULL) && (current->content != pointer))
    current = current->next;

  if (current == NULL || current->isFree) {
    fprintf(stderr, "Error: trying to free unallocated pointer\n");
    exit(1);
  }

  current->isFree = true;

  // Find start of free blocks chain
  HeapBlock *startBlock = current;
  while (startBlock->previous != NULL && startBlock->previous->isFree) {
    startBlock = startBlock->previous;
  }

  // Find end of free blocks chain
  HeapBlock *endBlock = current;
  while (endBlock->next != NULL && endBlock->next->isFree) {
    endBlock = endBlock->next;
  }

  // If start and end are identical, it's inbetween non free or nulls
  if (startBlock == endBlock)
    return;

  size_t newSize = startBlock->size;
  HeapBlock *walkBlock = startBlock->next;
  while (walkBlock != endBlock->next) {
    newSize += walkBlock->size + sizeof(HeapBlock);
    walkBlock = walkBlock->next;
  }

  startBlock->size = ((void *)endBlock + endBlock->size - (void *)startBlock);
  startBlock->isFree = true;
  startBlock->content = startBlock + sizeof(HeapBlock);
  startBlock->next = endBlock->next;
  if (startBlock->next != NULL)
    startBlock->next->previous = startBlock;
}

void *memRealloc(void *pointer, size_t newSize) {
  if (heap.first == NULL)
    initHeap();

  HeapBlock *current = heap.first;

  while ((current != NULL) && (current->content != pointer))
    current = current->next;

  // Not allocated yet, do so
  if (current == NULL)
    return memAlloc(newSize);

  if (newSize == current->size)
    return current; // Thanks, come again

  if (newSize < current->size) { // Smaller
    if ((current->size - newSize) < sizeof(HeapBlock) + 1)
      return current->content; // can't even fit a HeapBlock in the freed space,
                               // so leave as is

    HeapBlock *newBlock = current->content + newSize;
    newBlock->size = current->size - (newSize + sizeof(HeapBlock));
    newBlock->isFree = true;
    newBlock->content = newBlock + sizeof(HeapBlock);
    newBlock->previous = current;
    newBlock->next = current->next;

    current->size = newSize;
    current->next = newBlock;

    return current;
  } else { // Larger
    if (current->next == NULL) {
      fprintf(stderr, "Error: not enough memory\n");
      exit(1);
    }

    // Relocate - TODO Can be optimized quite a lot
    void *newLoc = memAlloc(newSize);
    memcpy(newLoc, current->content, current->size);

    return newLoc;
  }
}

void dumpHeapBlock(HeapBlock *block) {
  printf("=== Heap Block Dump\n");
  printf("Address: %p\n", block);
  printf("Size: %zu\n", block->size);
  printf("IsFree: %b\n", block->isFree);
  printf("Content: %p\n", block->content);
  printf("Previous: %p\n", block->previous);
  printf("Next: %p\n", block->next);
  printf("=== End Heap Block Dump\n");
}

void dumpHeap() {
  if (heap.first == NULL)
    initHeap();

  printf("== Heap Dump\n");

  HeapBlock *current = heap.first;
  while (current != NULL) {
    dumpHeapBlock(current);
    current = current->next;
  }

  printf("== End Heap Dump\n");
}

#endif

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    memFree(pointer);
    return NULL;
  }

  void *result = memRealloc(pointer, newSize);

  if (result == NULL)
    exit(1);

  return result;
}
