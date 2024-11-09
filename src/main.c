#include "chunk.h"
#include "debug.h"

#ifdef MANUAL_MEMORY_MANAGEMENT
#include <stdio.h>
#endif

int main(int argc, const char *argv[]) {
#ifdef MANUAL_MEMORY_MANAGEMENT
  printf("WARNING: using home-made manual memory management.\n");
#endif

  Chunk chunk;

  initChunk(&chunk);

  for (int i = 0; i < 50; i++) {
    writeConstant(&chunk, i + 1.2, i);
  }

  writeChunk(&chunk, OP_RETURN, 52);

  disassembleChunk(&chunk, "test chunk");

  freeChunk(&chunk);

  return 0;
}
