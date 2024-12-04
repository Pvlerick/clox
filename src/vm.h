#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "debug.h"
#include "stack.h"
#include "table.h"

typedef struct {
  Chunk* chunk;
  uint8_t* ip;
  Stack stack;
  Table strings;
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);

#endif
