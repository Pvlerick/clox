#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "debug.h"
#include "object.h"
#include "stack.h"
#include "table.h"

#define FRAMES_MAX 64

#define GET_CALLEE(frame) (frame->type == CALLEE_CLOSURE ? frame->as.closure->function : frame->as.function)

typedef enum {
  CALLEE_FUNCTION,
  CALLEE_CLOSURE,
} CalleeType;

typedef struct {
  CalleeType type;
  union {
    ObjClosure *closure;
    ObjFunction *function;
  } as;
  uint8_t *ip;
  int stackIndex;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  Stack stack;
  Table globals;
  Table strings;
  ObjString *initString;
  ObjUpvalue *openUpvalues;
  size_t bytesAllocated;
  size_t nextGC;
  Obj *objects;
  int grayCount;
  int grayCapacity;
  Obj **grayStack;
  bool markValue;
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
void push(Value value);
Value pop();

#endif
