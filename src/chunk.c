#include <stdint.h>

#include "chunk.h"
#include "line.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

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
    // disableGC();
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    // enableGC();
  }

  chunk->code[chunk->count] = byte;
  addInstructionLine(&chunk->lines, chunk->count, line);
  chunk->count++;
}

static ConstRef makeConstRef(int index) {
  if (index < 256) {
    ConstRef ref = {.type = CONST, {.constant = index}};
    return ref;
  } else {
    ConstRef ref = {.type = CONST_LONG, {.longConstant = index}};
    return ref;
  }
}

ConstRef addConstant(Chunk *chunk, Value value) {
  // Look in the constant table if the constant already exists
  for (int i = 0; i < chunk->constants.count; i++)
    if (valuesEqual(*(chunk->constants.values + i), value)) {
      return makeConstRef(i);
    }

  push(value);
  writeValueArray(&chunk->constants, value);
  pop();

  return makeConstRef(chunk->constants.count - 1);
}

int getLine(Chunk *chunk, int offset) {
  return getInstructionLine(&chunk->lines, offset);
}
