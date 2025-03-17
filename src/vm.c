#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "stack.h"
#include "table.h"
#include "value.h"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

#define INITIAL_NEXT_GC (1024)

VM vm;

static void runtimeError(const char *format, ...);
static void defineNative(const char *name, NativeFn fun, int arity);

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value envNative(int argCount, Value *args) {
  if (!IS_STRING(*args)) {
    runtimeError("argument to 'env' native function must be a string.");
  }

  const char *name = copyString(AS_STRING(*args));
  const char *var = getenv(name);
  free((void *)name);

  if (var != nullptr) {
    return OBJ_VAL(newOwnedString(var, strlen(var)));
  }

  return NIL_VAL;
}

static Value randNative(int argCout, Value *args) {
  if (!IS_NUMBER(*args) || !IS_NUMBER(*(args + 1))) {
    runtimeError("arguments to 'rand' native function must be numbers.");
  }

  double a = AS_NUMBER(*args);
  double b = AS_NUMBER(*(args + 1));

  if (a >= b) {
    runtimeError("second argument to 'rand' native function must be strictly "
                 "larger than first argument.");
  }

  if (a < INT_MIN || a > INT_MAX || b < INT_MIN || b > INT_MAX) {
    runtimeError(
        "arguments to 'rand' native function must be in integer range.");
  }

  int lower = (int)round(a);
  int upper = (int)round(b);

  srand(time(nullptr));

  return NUMBER_VAL(rand() % upper + lower);
}

static Value exitNative(int argCout, Value *args) {
  if (!IS_NUMBER(*args)) {
    runtimeError("argument to 'exit' must be an integer.");
  }

  exit((int)round(AS_NUMBER(*args)));
}

void initVM() {
  initStack(&vm.stack);
  vm.objects = nullptr;
  vm.bytesAllocated = 0;
  vm.nextGC = INITIAL_NEXT_GC;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = nullptr;
  vm.markValue = true;

  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.initString = nullptr;
  vm.initString = newOwnedString("init", 4);

  defineNative("clock", clockNative, 0);
  defineNative("env", envNative, 1);
  defineNative("rand", randNative, 2);
  defineNative("exit", exitNative, 1);
}

void freeVM() {
  freeStack(&vm.stack);
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = nullptr;
  freeObjects();
}

static void resetStack() {
  freeStack(&vm.stack);
  vm.frameCount = 0;
  vm.openUpvalues = nullptr;
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *fun = GET_CALLEE(frame);
    size_t instruction = frame->ip - fun->chunk.code - 1;
    fprintf(stderr, "[line %d] in ",
            getInstructionLine(&fun->chunk.lines, instruction));
    if (fun->name == nullptr) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%.*s()\n", fun->name->length, fun->name->content);
    }
  }

  resetStack();
}

void push(Value value) { pushOnStack(&vm.stack, value); }
Value pop() { return popFromStack(&vm.stack); }

static Value peek(int distance) { return peekFromStack(&vm.stack, distance); }

static void defineNative(const char *name, NativeFn fun, int arity) {
  push(OBJ_VAL(newOwnedString(name, strlen(name))));
  push(OBJ_VAL(newNative(fun, arity)));
  tableSet(&vm.globals, AS_STRING(peek(1)), peek(0));
  pop();
  pop();
}

static bool callFunction(ObjFunction *fun, int argCount) {
  if (argCount != fun->arity) {
    runtimeError("Expected %d arguments but got %d for <fn %.*s>", fun->arity,
                 argCount, fun->name->length, getCString(fun->name));
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->type = CALLEE_FUNCTION;
  frame->as.function = fun;
  frame->ip = fun->chunk.code;
  frame->stackIndex = vm.stack.count - argCount - 1;

  return true;
}

static bool callClosure(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d for <fn %.*s>",
                 closure->function->arity, argCount,
                 closure->function->name->length,
                 getCString(closure->function->name));
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->type = CALLEE_CLOSURE;
  frame->as.closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->stackIndex = vm.stack.count - argCount - 1;

  return true;
}

static bool call(Obj *obj, int argCount) {
  switch (obj->type) {
  case OBJ_FUNCTION:
    return callFunction((ObjFunction *)obj, argCount);
  case OBJ_CLOSURE:
    return callClosure((ObjClosure *)obj, argCount);
  default:
    runtimeError("Unknown bound method type.");
    return false;
  }
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_BOUND_METHOD:
      ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
      vm.stack.top[-argCount - 1] = bound->receiver;
      return call(bound->method, argCount);
    case OBJ_CLASS:
      ObjClass *klass = AS_CLASS(callee);
      vm.stack.top[-argCount - 1] = OBJ_VAL(newInstance(klass));
      if (klass->init != nullptr) {
        return call(klass->init, argCount);
      } else if (argCount != 0) {
        runtimeError("Expect 0 arguments but got %d.", argCount);
        return false;
      } else {
        return true;
      }
    case OBJ_FUNCTION:
      return callFunction(AS_FUNCTION(callee), argCount);
    case OBJ_CLOSURE:
      return callClosure(AS_CLOSURE(callee), argCount);
    case OBJ_NATIVE:
      ObjNative *native = AS_NATIVE(callee);
      if (native->arity != argCount) {
        runtimeError("Expected %d arguments but got %d for native function",
                     native->arity, argCount);
        return false;
      }
      Value result = native->function(argCount, vm.stack.top - argCount);
      stackDrop(&vm.stack, argCount + 1);
      push(result);
      return true;
    default:
      runtimeError("Unexpected OBJ_TYPE value: %d.", OBJ_TYPE(callee));
      break;
    }
  }

  runtimeError("Can only call function and classes.");
  return false;
}

static bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%.*s'.", name->length, getCString(name));
    return false;
  }
  return call(AS_OBJ(method), argCount);
}

static bool invoke(ObjString *name, int argCount) {
  Value receiver = peek(argCount);

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(receiver);
  return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass *klass, ObjString *name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    return false;
  }

  ObjBoundMethod *bound = newBoundMethod(peek(0), AS_OBJ(method));

  pop();
  push(OBJ_VAL(bound));

  return true;
}

static ObjUpvalue *captureUpvalue(int stackIndex) {
  ObjUpvalue *prevUpvalue = nullptr;
  ObjUpvalue *upvalue = vm.openUpvalues;
  while (upvalue != nullptr && upvalue->stackIndex > stackIndex) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != nullptr && upvalue->stackIndex == stackIndex) {
    return upvalue;
  }

  ObjUpvalue *createdUpvalue = newUpvalue(stackIndex);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == nullptr) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalue(int valueStackIndex) {
  while (vm.openUpvalues != nullptr &&
         vm.openUpvalues->stackIndex >= valueStackIndex) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = vm.stack.values[upvalue->stackIndex];
    upvalue->stackIndex = -1;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjString *name) {
  Value method = peek(0);
  ObjClass *klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}

static void defineInit() {
  Value init = peek(0);
  ObjClass *klass = AS_CLASS(peek(1));
  klass->init = AS_OBJ(init);
  pop();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  ObjString *result =
      allocateString(a->length + b->length, 2, toStringRef(a), toStringRef(b));

  pop();
  pop();

  push(OBJ_VAL(result));
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  uint8_t register *ip = frame->ip;

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((*(ip - 2)) << 8) | *(ip - 1))
#define READ_CONSTANT() (GET_CALLEE(frame)->chunk.constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT()                                                   \
  (GET_CALLEE(frame)->chunk.constants.values[READ_SHORT()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_STRING_LONG() AS_STRING(READ_LONG_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      frame->ip = ip;                                                          \
      runtimeError("Operand must be numbers.");                                \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

#ifdef DEBUG_TRACE_EXECUTION
  debug("## EXECUTION TRACE START ##\n");
#endif

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (int i = 0; i < vm.stack.count; i++) {
      printf("[ ");
      printValue(vm.stack.values[i]);
      printf(" ]");
    }
    printf("\n");
    ObjFunction *callee = GET_CALLEE(frame);
    disassembleInstruction(&callee->chunk, (int)(ip - callee->chunk.code));
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
    case OP_GET_PROP:
    case OP_GET_PROP_LONG: {
      if (!IS_INSTANCE(peek(0))) {
        frame->ip = ip;
        runtimeError("Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(0));
      ObjString *name =
          instruction == OP_GET_PROP ? READ_STRING() : READ_STRING_LONG();
      Value value;
      if (tableGet(&instance->fields, name, &value)) {
        pop();
        push(value);
      } else if (!bindMethod(instance->klass, name)) {
        push(NIL_VAL);
      }
      break;
    }
    case OP_GET_PROP_STR: {
      if (!IS_INSTANCE(peek(1))) {
        frame->ip = ip;
        runtimeError("Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(1));
      ObjString *name = AS_STRING(pop());
      Value value;
      pop();
      if (tableGet(&instance->fields, name, &value)) {
        push(value);
      } else {
        push(NIL_VAL);
      }
      break;
    }
    case OP_SET_PROP:
    case OP_SET_PROP_LONG: {
      if (!IS_INSTANCE(peek(1))) {
        frame->ip = ip;
        runtimeError("Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(1));
      ObjString *name =
          instruction == OP_SET_PROP ? READ_STRING() : READ_STRING_LONG();
      if (!IS_NIL(peek(0))) {
        tableSet(&instance->fields, name, peek(0));
      } else {
        tableDelete(&instance->fields, name);
      }
      Value value = pop();
      pop();
      push(value);
      break;
    }
    case OP_SET_PROP_STR: {
      if (!IS_INSTANCE(peek(2))) {
        frame->ip = ip;
        runtimeError("Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(2));
      ObjString *name = AS_STRING(peek(1));
      if (!IS_NIL(peek(0))) {
        tableSet(&instance->fields, name, peek(0));
      } else {
        tableDelete(&instance->fields, name);
      }
      Value value_set_prop_str = pop();
      pop();
      pop();
      push(value_set_prop_str);
      break;
    }
    case OP_GET_SUPER:
    case OP_GET_SUPER_LONG: {
      ObjString *name =
          instruction == OP_GET_SUPER ? READ_STRING() : READ_STRING_LONG();
      ObjClass *superclass = AS_CLASS(pop());
      if (!bindMethod(superclass, name))
        return INTERPRET_RUNTIME_ERROR;
      break;
    }
    case OP_EQUAL: {
      Value a = pop();
      Value b = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_CMP: {
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, peek(0))));
      break;
    }
    case OP_POP:
      pop();
      break;
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(stackGet(&vm.stack, frame->stackIndex + slot));
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      stackSet(&vm.stack, frame->stackIndex + slot, peek(0));
      break;
    }
    case OP_GET_GLOBAL:
    case OP_GET_GLOBAL_LONG: {
      ObjString *name =
          instruction == OP_GET_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        frame->ip = ip;
        runtimeError("Undefined variable '%.*s'.", name->length,
                     getCString(name));
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_DEFINE_GLOBAL:
    case OP_DEFINE_GLOBAL_LONG: {
      ObjString *name =
          instruction == OP_DEFINE_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_SET_GLOBAL:
    case OP_SET_GLOBAL_LONG: {
      ObjString *name =
          instruction == OP_SET_GLOBAL ? READ_STRING() : READ_STRING_LONG();
      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name);
        frame->ip = ip;
        runtimeError("Undefined variable '%.*s'.", name->length,
                     getCString(name));
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      ObjUpvalue *upvalue = frame->as.closure->upvalues[slot];
      if (upvalue->stackIndex != -1) {
        push(vm.stack.values[upvalue->stackIndex]);
      } else {
        push(upvalue->closed);
      }
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      frame->as.closure->upvalues[slot]->stackIndex = vm.stack.count - 1;
      break;
    }
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
        frame->ip = ip;
        runtimeError("Operands must be two numbers or two strings.");
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
        frame->ip = ip;
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
      ip += offset_j;
      break;
    case OP_JUMP_IF_FALSE:
      uint16_t offset_jif = READ_SHORT();
      if (isFalsey(peek(0)))
        ip += offset_jif;
      break;
    case OP_LOOP:
      uint16_t offset_loop = READ_SHORT();
      ip -= offset_loop;
      break;
    case OP_CALL:
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame->ip = ip;
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    case OP_INVOKE:
    case OP_INVOKE_LONG: {
      ObjString *method =
          instruction == OP_INVOKE ? READ_STRING() : READ_STRING_LONG();
      int argCout = READ_BYTE();
      if (!invoke(method, argCout))
        return INTERPRET_RUNTIME_ERROR;
      frame->ip = ip;
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    }
    case OP_SUPER_INVOKE:
    case OP_SUPER_INVOKE_LONG: {
      ObjString *method =
          instruction == OP_SUPER_INVOKE ? READ_STRING() : READ_STRING_LONG();
      int argCount = READ_BYTE();
      ObjClass *superclass = AS_CLASS(pop());
      if (!invokeFromClass(superclass, method, argCount))
        return INTERPRET_RUNTIME_ERROR;
      frame->ip = ip;
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *fun = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(fun);
      push(OBJ_VAL(closure));
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->stackIndex + index);
        } else {
          closure->upvalues[i] = frame->as.closure->upvalues[index];
        }
      }
      break;
    }
    case OP_CLOSURE_LONG: {
      ObjFunction *fun = AS_FUNCTION(READ_LONG_CONSTANT());
      ObjClosure *closure = newClosure(fun);
      push(OBJ_VAL(closure));
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->stackIndex + index);
        } else {
          closure->upvalues[i] = frame->as.closure->upvalues[index];
        }
      }
      break;
    }
    case OP_CLOSE_UPVALUE:
      closeUpvalue(vm.stack.count - 1);
      pop();
      break;
    case OP_RETURN:
      Value result = pop();
      closeUpvalue(frame->stackIndex);
      vm.frameCount--;

      if (vm.frameCount == 0) {
        pop();

#ifdef DEBUG_TRACE_EXECUTION
        debug("## EXECUTION TRACE END ##\n");
#endif

        return INTERPRET_OK;
      }

      stackReset(&vm.stack, frame->stackIndex);
      push(result);
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    case OP_CLASS:
      push(OBJ_VAL(newClass(READ_STRING())));
      break;
    case OP_CLASS_LONG:
      push(OBJ_VAL(newClass(READ_STRING_LONG())));
      break;
    case OP_INHERIT: {
      Value superclass = peek(1);
      if (!IS_CLASS(superclass)) {
        frame->ip = ip;
        runtimeError("Superclass must be a class.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjClass *sublcass = AS_CLASS(peek(0));
      tableAddAll(&AS_CLASS(superclass)->methods, &sublcass->methods);
      pop();
      break;
    }
    case OP_METHOD:
      defineMethod(READ_STRING());
      break;
    case OP_METHOD_LONG:
      defineMethod(READ_STRING_LONG());
      break;
    case OP_INIT:
      defineInit();
      break;

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
  ObjFunction *fun = compile(source);

  if (fun == nullptr) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJ_VAL(fun));
  callFunction(fun, 0);

  return run();
}
