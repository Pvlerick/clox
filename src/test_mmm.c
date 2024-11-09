#include "memory.h"
#include "testing.h"
#include <stdio.h>

#define MANUAL_MEMORY_MANAGEMENT 1;

typedef struct s {
  bool isFull;
  int size;
} TestStruct;

void testAllocateSimple() {
  printf("=== testAllocateSimple\n");

  void *ptr1 = memAlloc(42);  // 40 + 42 = 82
  void *ptr2 = memAlloc(80);  // 40 + 80 = 120
  void *ptr3 = memAlloc(100); // 40 + 100 = 140

  // dumpHeap();

  ASSERT_EQ_PTR(ptr2, ptr1 + 82);
  ASSERT_EQ_PTR(ptr3, ptr1 + 202);
}

void testAllocateThenFree() {
  printf("=== testAllocateThenFree\n");

  void *ptr = memAlloc(100);
  memFree(ptr);

  // dumpHeap();
}

void testAllocateThenFreeInvalid() {
  printf("=== testAllocateThenFreeInvalid\n");

  void *ptr = memAlloc(100);
  memFree(ptr + 5);
}

void testAllocateThenFreeThenAllocateSameSize() {
  printf("=== testAllocateThenFreeThenAllocateSameSize\n");

  void *ptr1 = memAlloc(42);
  void *ptr2 = memAlloc(80);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);

  // dumpHeap();

  void *ptr4 = memAlloc(80);

  // dumpHeap();
}

void testAllocateThenFreeThenAllocateSmaller() {
  printf("=== testAllocateThenFreeThenAllocateSmaller\n");

  void *ptr1 = memAlloc(42);
  void *ptr2 = memAlloc(80);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);

  // dumpHeap();

  void *ptr4 = memAlloc(10);
  void *ptr5 = memAlloc(29); // Will allocate a new block at the end

  // dumpHeap();
}

void testAllocateThenFreeFirstTwo() {
  printf("=== testAllocateThenFreeFirstOfTwo\n");

  void *ptr1 = memAlloc(75);
  void *ptr2 = memAlloc(150);
  void *ptr3 = memAlloc(100);
  memFree(ptr1);
  memFree(ptr2);

  // dumpHeap();
}

void testAllocateThenFreeSecondThenFirst() {
  printf("=== testAllocateThenFreeSecondOfTwo\n");

  void *ptr1 = memAlloc(30);
  void *ptr2 = memAlloc(60);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);
  memFree(ptr1);

  // dumpHeap();
}

void testAllocateThenFreeAll() {
  printf("=== testAllocateThenFreeAll\n");

  void *ptr1 = memAlloc(30);
  void *ptr2 = memAlloc(60);
  void *ptr3 = memAlloc(100);
  void *ptr4 = memAlloc(200);
  void *ptr5 = memAlloc(400);

  // dumpHeap();

  memFree(ptr1);
  memFree(ptr2);

  // dumpHeap();

  memFree(ptr3);
  memFree(ptr4);

  // dumpHeap();

  memFree(ptr5);

  // dumpHeap();
}

int main() {
  printf("Testing Manual Memory Management\n");

  testAllocateSimple();
  testAllocateThenFree();
  // testAllocateThenFreeInvalid();
  testAllocateThenFree();
  testAllocateThenFreeThenAllocateSameSize();
  testAllocateThenFreeThenAllocateSmaller();
  testAllocateThenFreeFirstTwo();
  testAllocateThenFreeSecondThenFirst();
  testAllocateThenFreeAll();

  return 0;
}
