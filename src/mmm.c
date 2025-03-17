#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

#define WORD_SIZE 8
#define ALIGN_TO_WORD_SIZE(x) (((((x) - 1) >> 3) << 3) + WORD_SIZE)
#define HEAP_BLOCK_SIZE sizeof(HeapBlock)

void *sbrk(intptr_t increment);

typedef struct HeapBlock {
  size_t size;
  bool isFree;
  void *content;
  struct HeapBlock *previous;
  struct HeapBlock *next;
} HeapBlock;

typedef struct {
  HeapBlock *first;
} Heap;

#define HEAP_MAX (1024 * 512)

Heap heap = {.first = nullptr};

void initHeap() {
  trace("MEM: heap size is %d bytes\n", HEAP_MAX);
  trace("MEM: heap block size is %lu bytes\n", HEAP_BLOCK_SIZE);

  void *heapStart = sbrk(HEAP_MAX);

  HeapBlock *first = heapStart;
  first->size = HEAP_MAX - HEAP_BLOCK_SIZE;
  first->isFree = true;
  first->content = heapStart + HEAP_BLOCK_SIZE;
  first->previous = nullptr;
  first->next = nullptr;

  heap.first = first;
}

void dumpHeapBlock(HeapBlock *block) {
  trace("=== Heap Block Dump\n");
  trace("Address: %p\n", block);
  trace("Size: %zu\n", block->size);
  trace("IsFree: %b\n", block->isFree);
  trace("Content: %p\n", block->content);
  trace("Previous: %p\n", block->previous);
  trace("Next: %p\n", block->next);
  trace("=== End Heap Block Dump\n");
}

void dumpHeap() {
  if (heap.first == nullptr)
    initHeap();

  trace("== Heap Dump\n");

  HeapBlock *current = heap.first;
  while (current != nullptr) {
    dumpHeapBlock(current);
    current = current->next;
  }

  trace("== End Heap Dump\n");
}

void checkHeapIntegrity() {
  size_t totalSize = 0;
  HeapBlock *current = heap.first;
  HeapBlock *previous = nullptr;
  while (current != nullptr) {
    totalSize += current->size + HEAP_BLOCK_SIZE;
    previous = current;
    current = current->next;

    if (current != nullptr && current->previous != previous)
      fprintf(stderr, "Error: previous reference in block is invalid\n");
  }

  if (totalSize != HEAP_MAX) {
    fprintf(stderr,
            "Error: total heap size incorrect; expected %u but got %lu\n",
            HEAP_MAX, totalSize);
  }
}

void *__wrap_malloc(size_t size) {
  if (heap.first == nullptr)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  size_t alignedSize = ALIGN_TO_WORD_SIZE(size);

  trace("MEM: allocation request for %zu bytes, aligned size is %zu bytes\n",
        size, alignedSize);

  HeapBlock *firstSuitable = heap.first;
  while ((firstSuitable != nullptr) &&
         !(firstSuitable->isFree &&
           ((firstSuitable->size == alignedSize) ||
            firstSuitable->size > alignedSize + HEAP_BLOCK_SIZE)))
    firstSuitable = firstSuitable->next;

  if (firstSuitable == nullptr) {
    err(ENOMEM,
        "Error: out of memory - no suitable block found on the heap to "
        "allocate %zu bytes\n",
        alignedSize);
  }

  trace("MEM: suitable block found at %p, block size is %lu\n", firstSuitable,
        firstSuitable->size);

  if (firstSuitable->size == alignedSize) {
    // Requested size perfectly match free size, just update the block
    firstSuitable->isFree = false;
    trace("MEM: allocated %zu bytes at %p, block is at %p\n", alignedSize,
          firstSuitable->content, firstSuitable);
    return firstSuitable->content;
  }

  HeapBlock *next = firstSuitable->content + alignedSize;
  next->size = firstSuitable->size - (HEAP_BLOCK_SIZE + alignedSize);
  next->isFree = true;
  next->content = firstSuitable->content + HEAP_BLOCK_SIZE + alignedSize;
  next->previous = firstSuitable;
  next->next = firstSuitable->next;
  if (firstSuitable->next != nullptr)
    firstSuitable->next->previous = next;

  firstSuitable->size = alignedSize;
  firstSuitable->isFree = false;
  firstSuitable->next = next;

  trace("MEM: allocated %zu bytes at %p, block is at %p\n", alignedSize,
        firstSuitable->content, firstSuitable);
  return firstSuitable->content;
}

void __wrap_free(void *ptr) {
  if (ptr == nullptr)
    return;

  if (heap.first == nullptr)
    err(EXIT_FAILURE,
        "Error: trying to free while heap has not been initialized yet\n");

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  HeapBlock *current = heap.first;

  while ((current != nullptr) && (current->content != ptr))
    current = current->next;

  if (current == nullptr) {
    err(EXIT_FAILURE,
        "Error: cannot free block at %p because it was not found\n", ptr);
  }

  if (current->isFree)
    err(EXIT_FAILURE, "Error: trying to free unallocated pointer %p\n", ptr);

  trace("MEM: freeing %d bytes at %p, block is at %p\n", current->size, ptr,
        current);

  current->isFree = true;

  // Find start of free blocks chain
  HeapBlock *startBlock = current;
  while (startBlock->previous != nullptr && startBlock->previous->isFree) {
    startBlock = startBlock->previous;
  }

  // Find end of free blocks chain
  HeapBlock *endBlock = current;
  while (endBlock->next != nullptr && endBlock->next->isFree) {
    endBlock = endBlock->next;
  }

  // If start and end are identical, it's inbetween non free or nulls
  if (startBlock == endBlock)
    return;

  size_t newSize = startBlock->size;
  HeapBlock *walkBlock = startBlock->next;
  while (walkBlock != endBlock->next) {
    newSize += walkBlock->size + HEAP_BLOCK_SIZE;
    walkBlock = walkBlock->next;
  }

  startBlock->size = ((void *)endBlock + endBlock->size - (void *)startBlock);
  startBlock->isFree = true;
  startBlock->content = (void *)startBlock + HEAP_BLOCK_SIZE;
  startBlock->next = endBlock->next;
  if (startBlock->next != nullptr)
    startBlock->next->previous = startBlock;
}

void *__wrap_realloc(void *ptr, size_t size) {
  if (heap.first == nullptr)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  size_t alignedNewSize = ALIGN_TO_WORD_SIZE(size);

  // Not allocated yet, do so
  if (ptr == nullptr)
    return malloc(alignedNewSize);

  HeapBlock *current = heap.first;

  while ((current != nullptr) && (current->content != ptr))
    current = current->next;

  if (current == nullptr) {
    fprintf(
        stderr,
        "Error: trying to reallocate non-existing pointer %p to %lu bytes\n",
        ptr, alignedNewSize);
    exit(1);
  }

  if (alignedNewSize == current->size)
    return current->content; // Thanks, come again

  if (alignedNewSize < current->size) { // Smaller
    size_t offset = current->size - alignedNewSize;

    if (current->next->isFree) {
      // Next is free: resize both
      memcpy((void *)current->next - offset, (void *)current->next,
             HEAP_BLOCK_SIZE);
      current->size = alignedNewSize;
      current->next = (void *)current->next - offset;
      current->next->size += offset;
      current->next->content -= offset;
      return current->content;
    } else if (offset <= HEAP_BLOCK_SIZE) {
      // Can't fint a HeapBlock in the freed space, don't do anything
      return current->content;
    } else {
      HeapBlock *newBlock = current->content + alignedNewSize;
      newBlock->size = current->size - (alignedNewSize + HEAP_BLOCK_SIZE);
      newBlock->isFree = true;
      newBlock->content = newBlock + HEAP_BLOCK_SIZE;
      newBlock->previous = current;
      newBlock->next = current->next;

      current->size = alignedNewSize;
      current->next = newBlock;

      return current->content;
    }
  } else { // Larger
    size_t offset = alignedNewSize - current->size;
    if (current->next->isFree &&
        current->next->size + HEAP_BLOCK_SIZE == offset) {
      // Next is free and has the exact size we need: absorb it
      current->size = alignedNewSize;
      current->next = current->next->next;
      current->next->previous = current;
      return current->content;
    } else if (current->next->isFree && current->next->size > offset) {
      // Next is free and has enough space: resize both
      memcpy((void *)current->next + offset, (void *)current->next,
             HEAP_BLOCK_SIZE);
      current->next = (void *)current->next + offset;
      current->next->previous = current;
      current->next->size -= offset;
      current->next->content += offset;
      current->size += offset;
      if (current->next->next != nullptr)
        current->next->next->previous = current->next;
      return current->content;
    } else {
      // In all other cases: alocate new and copy
      void *newLoc = malloc(alignedNewSize);
      memcpy(newLoc, current->content, current->size);

      free(current->content);

      return newLoc;
    }
  }
}
