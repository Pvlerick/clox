#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "line.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_LONG,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_LONG,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_LONG,
  OP_EQUAL,
  OP_CMP,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_CLOSURE,
  OP_CLOSURE_LONG,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  LineArray lines;
  ValueArray constants;
} Chunk;

typedef enum {
  CONST,
  CONST_LONG,
} ConsType;

typedef struct {
  ConsType type;
  union {
    uint8_t constant;
    int longConstant;
  } as;
} ConstRef;

typedef enum {
  VAR_GLOBAL,
  VAR_LOCAL,
} VariableType;

typedef struct {
  VariableType type;
  bool readonly;
  union {
    ConstRef global;
  } as;
} VariableRef;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
ConstRef addConstant(Chunk* chunk, Value value);
int getLine(Chunk *chunk, int offset);

#endif
