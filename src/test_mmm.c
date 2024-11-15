#include "memory.h"
#include "testing.h"
#include <stdio.h>

typedef struct s {
  bool isFull;
  int size;
} TestStruct;

void testAllocateSimple() {
  printf("====== AllocateSimple\n");

  void *ptr1 = memAlloc(42);  // 40 + 42 = 82
  void *ptr2 = memAlloc(80);  // 40 + 80 = 120
  void *ptr3 = memAlloc(100); // 40 + 100 = 140

  dumpHeap();

  ASSERT_EQ_PTR(ptr2, ptr1 + 82);
  ASSERT_EQ_PTR(ptr3, ptr1 + 202);
}

void testAllocateThenFree() {
  printf("====== AllocateThenFree\n");

  void *ptr = memAlloc(100);
  memFree(ptr);

  dumpHeap();
}

void testAllocateThenFreeInvalid() {
  printf("====== AllocateThenFreeInvalid\n");

  void *ptr = memAlloc(100);
  memFree(ptr + 5);
}

void testAllocateThenFreeThenAllocateSameSize() {
  printf("====== AllocateThenFreeThenAllocateSameSize\n");

  void *ptr1 = memAlloc(42);
  void *ptr2 = memAlloc(80);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);

  dumpHeap();

  void *ptr4 = memAlloc(80);

  dumpHeap();
}

void testAllocateThenFreeThenAllocateSmaller() {
  printf("====== AllocateThenFreeThenAllocateSmaller\n");

  void *ptr1 = memAlloc(42);
  void *ptr2 = memAlloc(80);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);

  dumpHeap();

  void *ptr4 = memAlloc(10);
  void *ptr5 = memAlloc(29); // Will allocate a new block at the end

  dumpHeap();
}

void testAllocateThenFreeFirstTwo() {
  printf("====== AllocateThenFreeFirstOfTwo\n");

  void *ptr1 = memAlloc(75);
  void *ptr2 = memAlloc(150);
  void *ptr3 = memAlloc(100);
  memFree(ptr1);
  memFree(ptr2);

  dumpHeap();
}

void testAllocateThenFreeSecondThenFirst() {
  printf("====== AllocateThenFreeSecondOfTwo\n");

  void *ptr1 = memAlloc(30);
  void *ptr2 = memAlloc(60);
  void *ptr3 = memAlloc(100);
  memFree(ptr2);
  memFree(ptr1);

  dumpHeap();
}

void testAllocateThenFreeAll() {
  printf("====== AllocateThenFreeAll\n");

  void *ptr1 = memAlloc(30);
  void *ptr2 = memAlloc(60);
  void *ptr3 = memAlloc(100);
  void *ptr4 = memAlloc(200);
  void *ptr5 = memAlloc(400);

  dumpHeap();

  memFree(ptr1);
  memFree(ptr2);

  dumpHeap();

  memFree(ptr3);
  memFree(ptr4);

  dumpHeap();

  memFree(ptr5);

  dumpHeap();
}

void testReallocateSimple() {
  printf("====== ReallocateSimple\n");

  void *ptr = memAlloc(100);

  dumpHeap();

  memRealloc(ptr, 200);

  dumpHeap();
}

void testReallocateOnlyOneByteOnTop() {
  printf("====== ReallocateOnlyOneByteOnTop\n");

  void *ptr1 = memAlloc(100);
  void *ptr2 = memAlloc(101);
  void *ptr3 = memAlloc(50);

  memFree(ptr2);

  dumpHeap();

  memRealloc(ptr1, 200);

  dumpHeap();
}

void testReallocateNextBlockHasExactlyTheRightSize() {
  printf("====== ReallocateNextBlockHasExactlyTheRightSize\n");

  void *ptr1 = memAlloc(100);
  void *ptr2 = memAlloc(60);
  void *ptr3 = memAlloc(50);

  memFree(ptr2);
  memRealloc(ptr1, 200);

  dumpHeap();
}

void testReallocateNextIsFreeButTooSmall() {
  printf("====== ReallocateNextIsFreeButTooSmall\n");

  void *ptr1 = memAlloc(100);
  void *ptr2 = memAlloc(59);
  void *ptr3 = memAlloc(50);

  memFree(ptr2);
  void *ptr4 = memRealloc(ptr1, 200);

  dumpHeap();

  ASSERT_NE_PTR(ptr1, ptr4);
}

void testReallocateNextIsNotFree() {
  printf("====== ReallocateNextIsNotFree\n");

  void *ptr1 = memAlloc(200);
  void *ptr2 = memAlloc(40);

  void *ptr3 = memRealloc(ptr1, 300);

  dumpHeap();

  ASSERT_NE_PTR(ptr1, ptr3);
}

void testReallocateWayTooMuch() {
  printf("====== ReallocateWayTooMuch\n");

  void *ptr = memAlloc(100);

  memRealloc(ptr, 500000);
}

void testReallocateSameSize() {
  printf("====== ReallocateSameSize\n");

  void *ptr1 = memAlloc(200);
  void *ptr2 = memRealloc(ptr1, 200);

  ASSERT_EQ_PTR(ptr1, ptr2);
}

void testReallocateSmaller() {
  printf("====== ReallocateSmaller\n");

  void *ptr1 = memAlloc(200);
  void *ptr2 = memRealloc(ptr1, 100);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr2);
}

void testReallocateFreedIsTooSmall() {
  printf("====== ReallocateFreedIsTooSmall\n");

  void *ptr1 = memAlloc(200);
  void *ptr2 = memAlloc(50);
  void *ptr3 = memRealloc(ptr1, 160);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr3);
}

void testReallocateEnoughForNewBlockAfter() {
  printf("====== ReallocateEnoughForNewBlockAfter\n");

  void *ptr1 = memAlloc(200);
  void *ptr2 = memAlloc(50);
  void *ptr3 = memRealloc(ptr1, 100);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr3);
}

void testCustom() {
  printf("====== Custom\n");

  void *ptr1 = memAlloc(64);
  void *ptr2 = memAlloc(8);
  void *ptr3 = memAlloc(96);
  ptr2 = memRealloc(ptr2, 16);
  ptr1 = memRealloc(ptr1, 128);
  ptr2 = memRealloc(ptr2, 32);
  ptr3 = memRealloc(ptr3, 192);
  ptr1 = memRealloc(ptr1, 256);
  ptr2 = memRealloc(ptr2, 64);
  ptr3 = memRealloc(ptr3, 384);
  ptr1 = memRealloc(ptr1, 512);
  ptr2 = memRealloc(ptr2, 128);
  ptr3 = memRealloc(ptr3, 768);
  ptr1 = memRealloc(ptr1, 1024);
  ptr2 = memRealloc(ptr2, 256);
  ptr3 = memRealloc(ptr3, 1536);
  ptr1 = memRealloc(ptr1, 2048);
  ptr3 = memRealloc(ptr3, 3072);
  ptr1 = memRealloc(ptr1, 4096);

  // dumpHeap();
  checkHeapIntegrity();

  memFree(ptr1);
  memFree(ptr2);
  memFree(ptr3);

  // dumpHeap();
}

int main() {
  printf("Testing Manual Memory Management\n");

  // testAllocateSimple();
  // testAllocateThenFree();
  // testAllocateThenFreeInvalid();
  // testAllocateThenFree();
  // testAllocateThenFreeThenAllocateSameSize();
  // testAllocateThenFreeThenAllocateSmaller();
  // testAllocateThenFreeFirstTwo();
  // testAllocateThenFreeSecondThenFirst();
  // testAllocateThenFreeAll();
  //
  // testReallocateSimple();
  // testReallocateOnlyOneByteOnTop();
  // testReallocateNextBlockHasExactlyTheRightSize();
  // testReallocateNextIsFreeButTooSmall();
  // testReallocateNextIsNotFree();
  //
  // testReallocateSameSize();
  //
  // testReallocateSmaller();
  // testReallocateFreedIsTooSmall();
  // testReallocateEnoughForNewBlockAfter();

  testCustom();

  return 0;
}
