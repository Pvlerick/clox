#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  Chunk chunk;

  initChunk(&chunk);

  for (int i = 0; i < 300; i++) {
    writeConstant(&chunk, i + 1.2, i);
  }

  writeChunk(&chunk, OP_RETURN, 42);

  disassembleChunk(&chunk, "test chunk");

  freeChunk(&chunk);

  return 0;
}
