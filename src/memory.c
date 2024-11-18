#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NO_MANUAL_MEMORY_MANAGEMENT

void *memRealloc(void *pointer, size_t newSize) {
  return realloc(pointer, newSize);
}

void memFree(void *pointer) { free(pointer); }

#else

#include "debug.h"

#define HEAP_MAX 65535

Heap heap = {.first = nullptr};

void initHeap() {
  debug("MEM: heap size: %d bytes\n", HEAP_MAX);
  debug("MEM: heap block size: %lu bytes\n", HEAP_BLOCK_SIZE);

  void *heapStart = malloc(HEAP_MAX);

  HeapBlock *first = heapStart;
  first->size = HEAP_MAX - HEAP_BLOCK_SIZE;
  first->isFree = true;
  first->content = heapStart + HEAP_BLOCK_SIZE;
  first->previous = nullptr;
  first->next = nullptr;

  heap.first = first;
}

void *memAlloc(size_t size) {
  if (heap.first == nullptr)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  size_t alignedSize = ALIGN_TO_WORD_SIZE(size);

  debug("MEM: allocation request for %zu bytes\n", alignedSize);

  HeapBlock *firstSuitable = heap.first;
  while (!(firstSuitable->isFree &&
           ((firstSuitable->size == alignedSize) ||
            firstSuitable->size > alignedSize + HEAP_BLOCK_SIZE)))
    firstSuitable = firstSuitable->next;

  debug("MEM: suitable block found at %p, block size: %lu\n", firstSuitable,
        firstSuitable->size);

  if (firstSuitable->size == alignedSize) {
    // Requested size perfectly match free size, just update the block
    firstSuitable->isFree = false;
    debug("MEM: allocated %zu bytes as %p\n", alignedSize,
          firstSuitable->content);
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

  debug("MEM: allocated %zu bytes as %p\n", alignedSize,
        firstSuitable->content);
  return firstSuitable->content;
}

void memFree(void *pointer) {
  if (heap.first == nullptr) {
    fprintf(stderr, "Error: trying to free unallocated pointer\n");
    exit(1);
  }

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  HeapBlock *current = heap.first;

  while ((current != nullptr) && (current->content != pointer))
    current = current->next;

  if (current == nullptr || current->isFree) {
    fprintf(stderr, "Error: trying to free unallocated pointer\n");
    exit(1);
  }

  debug("MEM: freeing: %p\n", pointer);

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

void *memRealloc(void *pointer, size_t newSize) {
  if (heap.first == nullptr)
    initHeap();

#ifdef DEBUG_TRACE_MEMORY
  checkHeapIntegrity();
#endif

  size_t alignedNewSize = ALIGN_TO_WORD_SIZE(newSize);

  // Not allocated yet, do so
  if (pointer == nullptr)
    return memAlloc(alignedNewSize);

  HeapBlock *current = heap.first;

  while ((current != nullptr) && (current->content != pointer))
    current = current->next;

  if (current == nullptr) {
    fprintf(
        stderr,
        "Error: trying to reallocate non-existing pointer %p to %lu bytes\n",
        pointer, alignedNewSize);
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
      void *newLoc = memAlloc(alignedNewSize);
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
  if (heap.first == nullptr)
    initHeap();

  debug("== Heap Dump\n");

  HeapBlock *current = heap.first;
  while (current != nullptr) {
    dumpHeapBlock(current);
    current = current->next;
  }

  debug("== End Heap Dump\n");
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

#endif

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    memFree(pointer);
    return nullptr;
  }

  void *result = memRealloc(pointer, newSize);

  if (result == nullptr)
    exit(1);

  return result;
}
