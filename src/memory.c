#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MANUAL_MEMORY_MANAGEMENT

void *memRealloc(void *pointer, size_t newSize) {
  return realloc(pointer, newSize);
}

void memFree(void *pointer) { free(pointer); }

#else

#include "debug.h"

#define HEAP_MAX 65535

Heap heap = {.first = NULL};

void initHeap() {
  debug("WARNING: using home-made manual memory management.\n");

  debug("MEM: heap size: %d bytes\n", HEAP_MAX);
  debug("MEM: heap block size: %lu bytes\n", sizeof(HeapBlock));

  void *heapStart = malloc(HEAP_MAX);

  HeapBlock *first = heapStart;
  first->size = HEAP_MAX - sizeof(HeapBlock);
  first->isFree = true;
  first->content = heapStart + sizeof(HeapBlock);
  first->previous = NULL;
  first->next = NULL;

  heap.first = first;
}

void *memAlloc(size_t size) {
  if (heap.first == NULL)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  debug("MEM: allocating %zu bytes\n", size);

  HeapBlock *firstSuitable = heap.first;
  while (!(firstSuitable->isFree &&
           ((firstSuitable->size == size) ||
            firstSuitable->size > size + sizeof(HeapBlock))))
    firstSuitable = firstSuitable->next;

  debug("MEM: found suitable block at %p\n", firstSuitable);

  if (firstSuitable->size == size) {
    // Requested size perfectly match free size, just update the block
    firstSuitable->isFree = false;
    return firstSuitable->content;
  }

  debug("MEM: block size: %lu; requested size: %lu\n", firstSuitable->size,
        size);

  HeapBlock *next = firstSuitable->content + size;
  debug("MEM: created next block at %p\n", next);
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

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  HeapBlock *current = heap.first;

  while ((current != NULL) && (current->content != pointer))
    current = current->next;

  if (current == NULL || current->isFree) {
    fprintf(stderr, "Error: trying to free unallocated pointer\n");
    exit(1);
  }

  debug("MEM: freeing: %p\n", pointer);

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
  startBlock->content = (void *)startBlock + sizeof(HeapBlock);
  startBlock->next = endBlock->next;
  if (startBlock->next != NULL)
    startBlock->next->previous = startBlock;
}

void *memRealloc(void *pointer, size_t newSize) {
  if (heap.first == NULL)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  // Not allocated yet, do so
  if (pointer == NULL)
    return memAlloc(newSize);

  HeapBlock *current = heap.first;

  while ((current != NULL) && (current->content != pointer))
    current = current->next;

  if (current == NULL) {
    fprintf(
        stderr,
        "Error: trying to reallocate non-existing pointer %p to %lu bytes\n",
        pointer, newSize);
    exit(1);
  }

  if (newSize == current->size)
    return current->content; // Thanks, come again

  if (newSize < current->size) { // Smaller
    size_t offset = current->size - newSize;

    if (current->next->isFree) {
      // Next is free: resize both
      memcpy((void *)current->next - offset, (void *)current->next,
             sizeof(HeapBlock));
      current->size = newSize;
      current->next = (void *)current->next - offset;
      current->next->size += offset;
      current->next->content -= offset;
      return current->content;
    } else if (offset <= sizeof(HeapBlock)) {
      // Can't fint a HeapBlock in the freed space, don't do anything
      return current->content;
    } else {
      HeapBlock *newBlock = current->content + newSize;
      newBlock->size = current->size - (newSize + sizeof(HeapBlock));
      newBlock->isFree = true;
      newBlock->content = newBlock + sizeof(HeapBlock);
      newBlock->previous = current;
      newBlock->next = current->next;

      current->size = newSize;
      current->next = newBlock;

      return current->content;
    }
  } else { // Larger
    size_t offset = newSize - current->size;
    if (current->next->isFree &&
        current->next->size + sizeof(HeapBlock) == offset) {
      // Next is free and has the exact size we need: absorb it
      current->size = newSize;
      current->next = current->next->next;
      current->next->previous = current;
      return current->content;
    } else if (current->next->isFree && current->next->size > offset) {
      // Next is free and has enough space: resize both
      memcpy((void *)current->next + offset, (void *)current->next,
             sizeof(HeapBlock));
      current->next = (void *)current->next + offset;
      current->next->previous = current;
      current->next->size -= offset;
      current->next->content += offset;
      current->size += offset;
      if (current->next->next != NULL)
        current->next->next->previous = current->next;
      return current->content;
    } else {
      // In all other cases: alocate new and copy
      void *newLoc = memAlloc(newSize);
      memcpy(newLoc, current->content, current->size);

      memFree(current->content);

      return newLoc;
    }
  }
}

void dumpHeapBlock(HeapBlock *block) {
  debug("=== Heap Block Dump\n");
  debug("Address: %p\n", block);
  debug("Size: %zu\n", block->size);
  debug("IsFree: %b\n", block->isFree);
  debug("Content: %p\n", block->content);
  debug("Previous: %p\n", block->previous);
  debug("Next: %p\n", block->next);
  debug("=== End Heap Block Dump\n");
}

void dumpHeap() {
  if (heap.first == NULL)
    initHeap();

  debug("== Heap Dump\n");

  HeapBlock *current = heap.first;
  while (current != NULL) {
    dumpHeapBlock(current);
    current = current->next;
  }

  debug("== End Heap Dump\n");
}

void checkHeapIntegrity() {
  size_t totalSize = 0;
  HeapBlock *current = heap.first;
  HeapBlock *previous = NULL;
  while (current != NULL) {
    totalSize += current->size + sizeof(HeapBlock);
    previous = current;
    current = current->next;

    if (current != NULL && current->previous != previous)
      fprintf(stderr, "Error: previous reference in block is invalid\n");
  }

  if (totalSize != HEAP_MAX) {
    fprintf(stderr,
            "Error: total heap size incorrect; expected %u but got %lu\n",
            HEAP_MAX, totalSize);
  }
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
