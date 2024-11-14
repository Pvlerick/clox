#include "vm.h"
#include "chunk.h"
#include "value.h"
#include <stdio.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

static void resetStack() { vm.stackTop = vm.stack; }

void initVM() { resetStack(); }
void freeVM() {}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT()                                                   \
  ({                                                                           \
    uint16_t *codeIndex = (uint16_t *)&vm.ip[0];                               \
    vm.ip += 2;                                                                \
    vm.chunk->constants.values[*codeIndex];                                    \
  })

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      push(READ_CONSTANT());
      break;
    }
    case OP_CONSTANT_LONG: {
      push(READ_LONG_CONSTANT());
      break;
    }
    case OP_RETURN: {
      printValue(pop());
      printf("\n");
      return INTERPRET_OK;
    }
    }
  }

#undef READ_LONG_CONSTANT
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult interpret(Chunk *chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}
