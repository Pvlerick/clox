#include "chunk.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  writeConstant(&chunk, 1, 0);
  writeConstant(&chunk, 2, 0);
  writeChunk(&chunk, OP_ADD, 0);
  writeConstant(&chunk, 3, 0);
  writeChunk(&chunk, OP_MULTIPLY, 0);
  writeConstant(&chunk, 4, 0);
  writeChunk(&chunk, OP_SUBTRACT, 0);
  writeConstant(&chunk, 5, 0);
  writeChunk(&chunk, OP_NEGATE, 0);
  writeChunk(&chunk, OP_DIVIDE, 0);

  writeChunk(&chunk, OP_RETURN, 0);

  interpret(&chunk);

  freeChunk(&chunk);
  freeVM();

  return 0;
}
