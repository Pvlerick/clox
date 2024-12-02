#include "value.h"
#include <stdarg.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#include "chunk.h"
#include "debug.h"

int debug(const char *format, ...) {
#ifdef DEBUG
  va_list arg;
  int done;

  va_start(arg, format);
  done = vfprintf(stdout, format, arg);
  va_end(arg);

  return done;
#else
  return 0;
#endif
}

void disassembleChunk(Chunk *chunk, const char *name) {
  debug("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char *name, int offset) {
  debug("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t codeIndex = chunk->code[offset + 1];
  debug("%-16s %4d '", name, codeIndex);
  printValue(chunk->constants.values[codeIndex]);
  debug("'\n");
  return offset + 2;
}

static int longConstantInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t *codeIndex = (uint16_t *)&chunk->code[offset + 1];
  debug("%-16s %4d '", name, *codeIndex);
  printValue(chunk->constants.values[*codeIndex]);
  debug("'\n");
  return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  debug("%04d ", offset);

  int line = getLine(chunk, offset);

  if (offset > 0 && line == getLine(chunk, offset - 1)) {
    debug("   | ");
  } else {
    debug("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_LONG:
    return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_ADD_ONE:
    return simpleInstruction("OP_ADD_ONE", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  default:
    debug("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
