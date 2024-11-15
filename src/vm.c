#include "vm.h"
#include "chunk.h"
#include "stack.h"
#include "value.h"
#include <stdio.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

void initVM() { initStack(&vm.stack); }

void freeVM() { freeStack(&vm.stack); }

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT()                                                   \
  ({                                                                           \
    uint16_t *codeIndex = (uint16_t *)&vm.ip[0];                               \
    vm.ip += 2;                                                                \
    vm.chunk->constants.values[*codeIndex];                                    \
  })
#define BINARY_OP(op)                                                          \
  do {                                                                         \
    double b = popFromStack(&vm.stack);                                        \
    double a = popFromStack(&vm.stack);                                        \
    pushOnStack(&vm.stack, a op b);                                            \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack.values; slot < vm.stack.top; slot++) {
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
      pushOnStack(&vm.stack, READ_CONSTANT());
      break;
    }
    case OP_CONSTANT_LONG: {
      pushOnStack(&vm.stack, READ_LONG_CONSTANT());
      break;
    }
    case OP_ADD: {
      BINARY_OP(+);
      break;
    }
    case OP_SUBTRACT: {
      BINARY_OP(-);
      break;
    }
    case OP_MULTIPLY: {
      BINARY_OP(*);
      break;
    }
    case OP_DIVIDE: {
      BINARY_OP(/);
      break;
    }
    case OP_NEGATE: {
      *(vm.stack.top - 1) = -(*(vm.stack.top - 1));
      // pushOnStack(&vm.stack, -popFromStack(&vm.stack));
      break;
    }
    case OP_RETURN: {
      popFromStack(&vm.stack);
      // printValue(popFromStack(&vm.stack));
      // printf("\n");
      return INTERPRET_OK;
    }
    }
  }

#undef BINARY_OP
#undef READ_LONG_CONSTANT
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult interpret(Chunk *chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}
