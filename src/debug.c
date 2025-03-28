#include "object.h"
#include "value.h"
#include <stdarg.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#ifdef TRACE
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

int trace(const char *format, ...) {
#ifdef TRACE
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

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  debug("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump =
      (uint16_t)((chunk->code[offset + 1] << 8) | (chunk->code[offset + 2]));
  debug("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
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

static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t codeIndex = chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset + 2];
  debug("%-16s %4d '", name, codeIndex);
  printValue(chunk->constants.values[codeIndex]);
  debug("'\n");
  return offset + 3;
}

static int longInvokeInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t *codeIndex = (uint16_t *)&chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset + 3];
  debug("%-16s %4d '", name, *codeIndex);
  printValue(chunk->constants.values[*codeIndex]);
  debug("'\n");
  return offset + 4;
}

static int closureParameters(Chunk *chunk, int offset, int codeIndex) {
  ObjFunction *fun = AS_FUNCTION(chunk->constants.values[codeIndex]);
  for (int i = 0; i < fun->upvalueCount; i++) {
    int isLocal = chunk->code[offset++];
    int index = chunk->code[offset++];
    debug("%04d      |                     %s %d\n", offset - 2,
          isLocal ? "local" : "upvalue", index);
  }

  return offset;
}

static int closureInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t codeIndex = chunk->code[offset + 1];
  debug("%-16s %4d ", name, codeIndex);
  printValue(chunk->constants.values[codeIndex]);
  debug("\n");

  return closureParameters(chunk, offset + 2, codeIndex);
}

static int longClosureInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t *codeIndex = (uint16_t *)&chunk->code[offset + 1];
  debug("%-16s %4d ", name, *codeIndex);
  printValue(chunk->constants.values[*codeIndex]);
  debug("\n");

  return closureParameters(chunk, offset + 3, *codeIndex);
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
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_GET_LOCAL:
    return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL_LONG:
    return longConstantInstruction("OP_GET_GLOBAL_LONG", chunk, offset);
  case OP_DEFINE_GLOBAL:
    return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL_LONG:
    return longConstantInstruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL_LONG:
    return longConstantInstruction("OP_SET_GLOBAL_LONG", chunk, offset);
  case OP_GET_UPVALUE:
    return byteInstruction("OP_GET_UPVALUE", chunk, offset);
  case OP_SET_UPVALUE:
    return byteInstruction("OP_SET_UPVALUE", chunk, offset);
  case OP_GET_PROP:
    return constantInstruction("OP_GET_PROP", chunk, offset);
  case OP_GET_PROP_LONG:
    return constantInstruction("OP_GET_PROP_LONG", chunk, offset);
  case OP_GET_PROP_STR:
    return byteInstruction("OP_GET_PROP_STR", chunk, offset);
  case OP_SET_PROP:
    return constantInstruction("OP_SET_PROP", chunk, offset);
  case OP_SET_PROP_LONG:
    return constantInstruction("OP_SET_PROP_LONG", chunk, offset);
  case OP_SET_PROP_STR:
    return byteInstruction("OP_SET_PROP_STR", chunk, offset);
  case OP_GET_SUPER:
    return constantInstruction("OP_GET_SUPER", chunk, offset);
  case OP_GET_SUPER_LONG:
    return constantInstruction("OP_GET_SUPER_LONG", chunk, offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_CMP:
    return simpleInstruction("OP_CMP", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
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
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  case OP_LOOP:
    return jumpInstruction("OP_LOOP", -1, chunk, offset);
  case OP_CALL:
    return byteInstruction("OP_CALL", chunk, offset);
  case OP_INVOKE:
    return invokeInstruction("OP_INVOKE", chunk, offset);
  case OP_INVOKE_LONG:
    return longInvokeInstruction("OP_INVOKE_LONG", chunk, offset);
  case OP_SUPER_INVOKE:
    return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
  case OP_SUPER_INVOKE_LONG:
    return longInvokeInstruction("OP_SUPER_INVOKE_LONG", chunk, offset);
  case OP_CLOSURE:
    return closureInstruction("OP_CLOSURE", chunk, offset);
  case OP_CLOSURE_LONG:
    return longClosureInstruction("OP_CLOSURE_LONG", chunk, offset);
  case OP_CLOSE_UPVALUE:
    return simpleInstruction("OP_CLOSE_UPVALUE", offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CLASS:
    return constantInstruction("OP_CLASS", chunk, offset);
  case OP_CLASS_LONG:
    return longConstantInstruction("OP_CLASS_LONG", chunk, offset);
  case OP_METHOD:
    return constantInstruction("OP_METHOD", chunk, offset);
  case OP_METHOD_LONG:
    return longConstantInstruction("OP_METHOD_LONG", chunk, offset);
  case OP_INIT:
    return constantInstruction("OP_INIT", chunk, offset);
  case OP_INHERIT:
    return simpleInstruction("OP_INHERIT", offset);
  default:
    debug("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
