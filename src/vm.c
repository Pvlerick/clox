#include "vm.h"
#include "chunk.h"
#include "value.h"
#include <stdio.h>

VM vm;

void initVM() {}
void freeVM() {}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT()                                                   \
  ({                                                                           \
    uint16_t *codeIndex = (uint16_t *)(*vm.ip);                                \
    printf("index: %hu", *codeIndex);                                          \
    READ_BYTE();                                                               \
    READ_BYTE();                                                               \
    vm.chunk->constants.values[*codeIndex];                                    \
  })
  // uint16_t *codeIndex = (uint16_t *)&chunk->code[offset + 1];
  // printf("index: %hu", *codeIndex);

  for (;;) {
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      printValue(constant);
      printf("\n");
      break;
    }
    // case OP_CONSTANT_LONG: {
    //   printf("long const\n");
    //   Value constant = READ_LONG_CONSTANT();
    //   printValue(constant);
    //   printf("\n");
    //   break;
    // }
    case OP_RETURN: {
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
