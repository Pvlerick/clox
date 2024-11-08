#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "debug.h"

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t codeIndex = chunk->code[offset + 1];
  printf("%-16s %4d '", name, codeIndex);
  printValue(chunk->consants.values[codeIndex]);
  printf("'\n");
  return offset + 2;
}

static int longConstantInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t *codeIndex = (uint16_t *)&chunk->code[offset + 1];
  printf("%-16s %4d '", name, *codeIndex);
  printValue(chunk->consants.values[*codeIndex]);
  printf("'\n");
  return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  int line = getLine(chunk, offset);

  if (offset > 0 && line == getLine(chunk, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_LONG:
    return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
