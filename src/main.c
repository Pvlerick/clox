#include "chunk.h"
#include "vm.h"
#include <stdio.h>
#include <time.h>

int main(int argc, const char *argv[]) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  writeConstant(&chunk, 1.2, 123);
  writeChunk(&chunk, OP_NEGATE, 123);

  writeChunk(&chunk, OP_RETURN, 0);

  clock_t start_time = clock();

  // push/pop * 100000000: 2.106770 seconds
  // in place * 100000000: 1.804757 seconds
  // push/pop * 1000000000: 21.069722 seconds
  // in place * 1000000000:  20.353537 seconds
  for (int i = 0; i < 1000000000; i++) {
    interpret(&chunk);
  }

  double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
  printf("Done in %f seconds\n", elapsed_time);

  freeChunk(&chunk);
  freeVM();

  return 0;
}
