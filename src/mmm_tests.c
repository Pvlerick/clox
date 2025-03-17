#include "mmm.h"
#include "testing.h"
#include <stdio.h>

void testAllocateSimple() {
  printf("====== AllocateSimple\n");

  void *ptr1 = __wrap_malloc(42); // 0 + 40 + 48 (42 aligned to word size) = 88
  void *ptr2 = __wrap_malloc(80); // 82 + 40 + 80 = 204
  void *ptr3 =
      __wrap_malloc(100); // 204 + 40 + 104 (100 aligned to word size) = 348

  dumpHeap();

  ASSERT_EQ_PTR(ptr2, ptr1 + 88);
  ASSERT_EQ_PTR(ptr3, ptr1 + 88 + 120);
}

void testAllocateThenFree() {
  printf("====== AllocateThenFree\n");

  void *ptr = __wrap_malloc(100);
  __wrap_free(ptr);

  dumpHeap();
}

void testAllocateThenFreeInvalid() {
  printf("====== AllocateThenFreeInvalid\n");

  void *ptr = __wrap_malloc(100);
  __wrap_free(ptr + 5);
}

void testAllocateThenFreeThenAllocateSameSize() {
  printf("====== AllocateThenFreeThenAllocateSameSize\n");

  void *ptr1 = __wrap_malloc(42);
  void *ptr2 = __wrap_malloc(80);
  void *ptr3 = __wrap_malloc(100);
  __wrap_free(ptr2);

  dumpHeap();

  void *ptr4 = __wrap_malloc(80);

  dumpHeap();
}

void testAllocateThenFreeThenAllocateSmaller() {
  printf("====== AllocateThenFreeThenAllocateSmaller\n");

  void *ptr1 = __wrap_malloc(42);
  void *ptr2 = __wrap_malloc(80);
  void *ptr3 = __wrap_malloc(100);
  __wrap_free(ptr2);

  dumpHeap();

  void *ptr4 = __wrap_malloc(10);
  void *ptr5 = __wrap_malloc(29); // Will allocate a new block at the end

  dumpHeap();
}

void testAllocateThenFreeFirstTwo() {
  printf("====== AllocateThenFreeFirstOfTwo\n");

  void *ptr1 = __wrap_malloc(75);
  void *ptr2 = __wrap_malloc(150);
  void *ptr3 = __wrap_malloc(100);
  __wrap_free(ptr1);
  __wrap_free(ptr2);

  dumpHeap();
}

void testAllocateThenFreeSecondThenFirst() {
  printf("====== AllocateThenFreeSecondOfTwo\n");

  void *ptr1 = __wrap_malloc(30);
  void *ptr2 = __wrap_malloc(60);
  void *ptr3 = __wrap_malloc(100);
  __wrap_free(ptr2);
  __wrap_free(ptr1);

  dumpHeap();
}

void testAllocateThenFreeAll() {
  printf("====== AllocateThenFreeAll\n");

  void *ptr1 = __wrap_malloc(30);
  void *ptr2 = __wrap_malloc(60);
  void *ptr3 = __wrap_malloc(100);
  void *ptr4 = __wrap_malloc(200);
  void *ptr5 = __wrap_malloc(400);

  dumpHeap();

  __wrap_free(ptr1);
  __wrap_free(ptr2);

  dumpHeap();

  __wrap_free(ptr3);
  __wrap_free(ptr4);

  dumpHeap();

  __wrap_free(ptr5);

  dumpHeap();
}

void testReallocateSimple() {
  printf("====== ReallocateSimple\n");

  void *ptr = __wrap_malloc(100);

  dumpHeap();

  __wrap_realloc(ptr, 200);

  dumpHeap();
}

void testReallocateOnlyOneByteOnTop() {
  printf("====== ReallocateOnlyOneByteOnTop\n");

  void *ptr1 = __wrap_malloc(100);
  void *ptr2 = __wrap_malloc(101);
  void *ptr3 = __wrap_malloc(50);

  __wrap_free(ptr2);

  dumpHeap();

  __wrap_realloc(ptr1, 200);

  dumpHeap();
}

void testReallocateNextBlockHasExactlyTheRightSize() {
  printf("====== ReallocateNextBlockHasExactlyTheRightSize\n");

  void *ptr1 = __wrap_malloc(100);
  void *ptr2 = __wrap_malloc(60);
  void *ptr3 = __wrap_malloc(50);

  __wrap_free(ptr2);
  __wrap_realloc(ptr1, 200);

  dumpHeap();
}

void testReallocateNextIsFreeButTooSmall() {
  printf("====== ReallocateNextIsFreeButTooSmall\n");

  void *ptr1 = __wrap_malloc(100);
  void *ptr2 = __wrap_malloc(59);
  void *ptr3 = __wrap_malloc(50);

  __wrap_free(ptr2);
  void *ptr4 = __wrap_realloc(ptr1, 200);

  dumpHeap();

  ASSERT_NE_PTR(ptr1, ptr4);
}

void testReallocateNextIsNotFree() {
  printf("====== ReallocateNextIsNotFree\n");

  void *ptr1 = __wrap_malloc(200);
  void *ptr2 = __wrap_malloc(40);

  void *ptr3 = __wrap_realloc(ptr1, 300);

  dumpHeap();

  ASSERT_NE_PTR(ptr1, ptr3);
}

void testReallocateWayTooMuch() {
  printf("====== ReallocateWayTooMuch\n");

  void *ptr = __wrap_malloc(100);

  __wrap_realloc(ptr, 500000);
}

void testReallocateSameSize() {
  printf("====== ReallocateSameSize\n");

  void *ptr1 = __wrap_malloc(200);
  void *ptr2 = __wrap_realloc(ptr1, 200);

  ASSERT_EQ_PTR(ptr1, ptr2);
}

void testReallocateSmaller() {
  printf("====== ReallocateSmaller\n");

  void *ptr1 = __wrap_malloc(200);
  void *ptr2 = __wrap_realloc(ptr1, 100);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr2);
}

void testReallocateFreedIsTooSmall() {
  printf("====== ReallocateFreedIsTooSmall\n");

  void *ptr1 = __wrap_malloc(200);
  void *ptr2 = __wrap_malloc(50);
  void *ptr3 = __wrap_realloc(ptr1, 160);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr3);
}

void testReallocateEnoughForNewBlockAfter() {
  printf("====== ReallocateEnoughForNewBlockAfter\n");

  void *ptr1 = __wrap_malloc(200);
  void *ptr2 = __wrap_malloc(50);
  void *ptr3 = __wrap_realloc(ptr1, 100);

  dumpHeap();

  ASSERT_EQ_PTR(ptr1, ptr3);
}

void testCustom() {
  printf("====== Custom\n");

  void *ptr1 = __wrap_malloc(64);
  void *ptr2 = __wrap_malloc(8);
  void *ptr3 = __wrap_malloc(96);
  ptr2 = __wrap_realloc(ptr2, 16);
  ptr1 = __wrap_realloc(ptr1, 128);
  ptr2 = __wrap_realloc(ptr2, 32);
  ptr3 = __wrap_realloc(ptr3, 192);
  ptr1 = __wrap_realloc(ptr1, 256);
  ptr2 = __wrap_realloc(ptr2, 64);
  ptr3 = __wrap_realloc(ptr3, 384);
  ptr1 = __wrap_realloc(ptr1, 512);
  ptr2 = __wrap_realloc(ptr2, 128);
  ptr3 = __wrap_realloc(ptr3, 768);
  ptr1 = __wrap_realloc(ptr1, 1024);
  ptr2 = __wrap_realloc(ptr2, 256);
  ptr3 = __wrap_realloc(ptr3, 1536);
  ptr1 = __wrap_realloc(ptr1, 2048);
  ptr3 = __wrap_realloc(ptr3, 3072);
  ptr1 = __wrap_realloc(ptr1, 4096);

  // dumpHeap();
  checkHeapIntegrity();

  __wrap_free(ptr1);
  __wrap_free(ptr2);
  __wrap_free(ptr3);

  // dumpHeap();
}

int main() {
  printf("Testing Manual Memory Management\n");

  testAllocateSimple();
  testAllocateThenFree();
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
