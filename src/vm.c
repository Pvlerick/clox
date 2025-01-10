#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "stack.h"
#include "table.h"
#include "value.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

void initVM() {
  initStack(&vm.stack);
  vm.objects = nullptr;
  initTable(&vm.globals);
  initTable(&vm.strings);
}

void freeVM() {
  freeStack(&vm.stack);
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
}

static void resetStack() { freeStack(&vm.stack); }

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines.items[instruction].line;
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

static void push(Value value) { pushOnStack(&vm.stack, value); }
static Value peek(int distance) { return peekFromStack(&vm.stack, distance); }
static Value pop() { return popFromStack(&vm.stack); }

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *b = AS_STRING(pop());
  ObjString *a = AS_STRING(pop());

  ObjString *result =
      allocateString(a->length + b->length, 2, toStringRef(a), toStringRef(b));

  push(OBJ_VAL(result));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT() vm.chunk->constants.values[READ_SHORT()]
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_STRING_LONG() AS_STRING(READ_LONG_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operand must be numbers.");                                \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
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
      push(READ_CONSTANT());
      break;
    case OP_CONSTANT_LONG:
      push(READ_LONG_CONSTANT());
      break;
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_EQUAL:
      Value a_eq = pop();
      Value b_eq = pop();
      push(BOOL_VAL(valuesEqual(a_eq, b_eq)));
      break;
    case OP_CMP:
      Value a_cmp = pop();
      push(BOOL_VAL(valuesEqual(a_cmp, peek(0))));
      break;
    case OP_POP:
      pop();
      break;
    case OP_GET_LOCAL:
      uint8_t slot_get = READ_BYTE();
      push(*(vm.stack.values + slot_get));
      break;
    case OP_SET_LOCAL:
      uint8_t slot_set = READ_BYTE();
      *(vm.stack.values + slot_set) = peek(0);
      break;
    case OP_GET_GLOBAL:
    case OP_GET_GLOBAL_LONG:
      ObjString *name_get =
          instruction == OP_GET_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      Value value_get;
      if (!tableGet(&vm.globals, name_get, &value_get)) {
        runtimeError("Undefined variable '%.*s'.", name_get->length,
                     getCString(name_get));
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value_get);
      break;
    case OP_DEFINE_GLOBAL:
    case OP_DEFINE_GLOBAL_LONG:
      ObjString *name_define =
          instruction == OP_DEFINE_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      tableSet(&vm.globals, name_define, peek(0));
      pop();
      break;
    case OP_SET_GLOBAL:
    case OP_SET_GLOBAL_LONG:
      ObjString *name_set =
          instruction == OP_SET_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      if (tableSet(&vm.globals, name_set, peek(0))) {
        tableDelete(&vm.globals, name_set);
        runtimeError("Undefined variable '%.*s'.", name_set->length,
                     getCString(name_set));
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_ADD:
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError("Operands must be two numbers of two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_PRINT:
      printValue(pop());
      printf("\n");
      break;
    case OP_JUMP:
      uint16_t offset_j = READ_SHORT();
      vm.ip += offset_j;
      break;
    case OP_JUMP_IF_FALSE:
      uint16_t offset_jif = READ_SHORT();
      if (isFalsey(peek(0)))
        vm.ip += offset_jif;
      break;
    case OP_LOOP:
      uint16_t offset_loop = READ_SHORT();
      vm.ip -= offset_loop;
      break;
    case OP_RETURN:
      return INTERPRET_OK;

#undef BINARY_OP
#undef READ_STRING_LONG
#undef READ_STRING
#undef READ_LONG_CONSTANT
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE
    }
    }
  }
}

InterpretResult interpret(const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  bool res = compile(source, &chunk);

  if (!res) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  // if (!compile(source, &chunk)) {
  //   freeChunk(&chunk);
  //   return INTERPRET_COMPILE_ERROR;
  // }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  InterpretResult result = run();

  freeChunk(&chunk);

  return result;
}
