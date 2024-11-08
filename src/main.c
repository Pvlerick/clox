#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  Chunk chunk;

  initChunk(&chunk);

  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 42);
  writeChunk(&chunk, constant, 42);
  writeChunk(&chunk, OP_RETURN, 42);

  disassembleChunk(&chunk, "test chunk");

  freeChunk(&chunk);

  return 0;
}
