#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  for (int i = 0; i < 3; i++) {
    writeConstant(&chunk, i + 0.2, i + 10);
  }

  writeChunk(&chunk, OP_RETURN, 52);

  disassembleChunk(&chunk, "test chunk");

  interpret(&chunk);

  freeChunk(&chunk);
  freeVM();

  return 0;
}
