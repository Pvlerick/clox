#include "chunk.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  writeConstant(&chunk, 4, 123);
  writeConstant(&chunk, 3, 123);
  writeChunk(&chunk, OP_NEGATE, 123);
  writeChunk(&chunk, OP_ADD, 123);
  writeConstant(&chunk, 2, 123);
  writeChunk(&chunk, OP_NEGATE, 123);
  writeChunk(&chunk, OP_MULTIPLY, 123);

  writeChunk(&chunk, OP_RETURN, 0);

  interpret(&chunk);

  freeChunk(&chunk);
  freeVM();

  return 0;
}
