#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "debug.h"
#include "stack.h"
#include "table.h"

#define FRAMES_MAX 64

typedef struct {
  ObjFunction *function;
  uint8_t *ip;
  int stackIndex;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  Stack stack;
  Table globals;
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
