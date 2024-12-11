#include <stdint.h>

#include "chunk.h"
#include "line.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = nullptr;
  initValueArray(&chunk->constants);
  initLineArray(&chunk->lines);
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  freeValueArray(&chunk->constants);
  freeLineArray(&chunk->lines);
  initChunk(chunk);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  addInstructionLine(&chunk->lines, chunk->count, line);
  chunk->count++;
}

ConstRef addConstant(Chunk *chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  int constant = chunk->constants.count - 1;
  ConstRef ref;
  if (constant < 256) {
    ConstRef ref = {.type = CONST, {.constant = constant}};
  } else {
    ConstRef ref = {.type = CONST_LONG, {.longConstant = constant}};
  }
  return ref;
}

int getLine(Chunk *chunk, int offset) {
  return getInstructionLine(&chunk->lines, offset);
}
