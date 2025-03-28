#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool captured;
  bool readonly;
} Local;

typedef struct {
  uint8_t index;
  bool readonly;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} FunctionType;

typedef struct {
  int capacity;
  int count;
  Local *items;
} LocalArray;

static void initLocalArray(LocalArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->items = nullptr;
}

static void writeLocalArray(LocalArray *array, Local item) {
  if (array->capacity < array->count + 1) {
    int oldCapactiy = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapactiy);
    array->items =
        GROW_ARRAY(Local, array->items, oldCapactiy, array->capacity);
  }

  array->items[array->count] = item;
  array->count++;
}

static void freeLocalArray(LocalArray *array) {
  FREE_ARRAY(Local, array->items, array->capacity);
  initLocalArray(array);
}

typedef struct {
  int start;
  int scopeDepth;
} LoopReference;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;
  Upvalue upvalues[UINT8_MAX];
  int scopeDepth;
  int loopDepth;
  LoopReference loopReference[UINT8_MAX];
  LocalArray locals;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler *enclosing;
  ObjString *superclass;
} ClassCompiler;

Parser parser;
Compiler *current = nullptr;
ClassCompiler *currentClass = nullptr;
Chunk *compilingChunk;

static Chunk *currentChunk() { return &current->function->chunk; }

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;

  parser.panicMode = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();

    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  } else {
    errorAtCurrent(message);
  }
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;

  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitReturn() {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

  emitByte(OP_RETURN);
}

static ConstRef makeConstant(Value value) {
  return addConstant(currentChunk(), value);
}

static void emitOpAndConstant(ConstRef ref, uint8_t opIfByte,
                              uint8_t opIfLong) {
  switch (ref.type) {
  case CONST:
    emitBytes(opIfByte, ref.as.constant);
    break;
  case CONST_LONG:
    emitByte(opIfLong);
    uint8_t *addr = (uint8_t *)&ref.as.longConstant;
    emitBytes(*(addr + 1), *addr);
    break;
  }
}

static ConstRef emitConstant(Value value) {
  ConstRef ref = makeConstant(value);
  emitOpAndConstant(ref, OP_CONSTANT, OP_CONSTANT_LONG);
  return ref;
}

static void emitClosure(Value value) {
  ConstRef ref = makeConstant(value);
  emitOpAndConstant(ref, OP_CLOSURE, OP_CLOSURE_LONG);
}

static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT8_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = nullptr;
  compiler->type = type;
  compiler->scopeDepth = 0;
  compiler->loopDepth = 0;
  initLocalArray(&compiler->locals);
  compiler->function = newFunction();
  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name =
        newOwnedString(parser.previous.start, parser.previous.length);
  }

  Local local;
  local.depth = 0;
  local.captured = false;
  local.readonly = true;

  if (type != TYPE_FUNCTION) {
    local.name.start = "this";
    local.name.length = 4;
  } else {
    local.name.start = "";
    local.name.length = 0;
  }

  writeLocalArray(&current->locals, local);
}

static ObjFunction *endCompiler() {
  emitReturn();
  ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    if (function->name != nullptr) {
      const char *functionName = copyString(function->name);
      disassembleChunk(currentChunk(), functionName);
      free((void *)functionName);
    } else {
      disassembleChunk(currentChunk(), "<script>");
    }
  }
#endif

  // Only free locals array when the top most block ends
  if (current->type == TYPE_SCRIPT) {
    freeLocalArray(&current->locals);
  }

  current = current->enclosing;

  return function;
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
  current->scopeDepth--;

  while (current->locals.count > 0 &&
         current->locals.items[current->locals.count - 1].depth >
             current->scopeDepth) {
    if (current->locals.items[current->locals.count - 1].captured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->locals.count--;
  }
}

static void beginLoop(int loopStart) {
  current->loopReference[current->loopDepth].start = loopStart;
  current->loopReference[current->loopDepth].scopeDepth = current->scopeDepth;
  current->loopDepth += 1;
}

static void endLoop() { current->loopDepth -= 1; }

static void statement();
static void declaration();
static ParseRule *getRule(TokenType);
static void parsePrecedence(Precedence);

static ConstRef identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(borrowString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;

  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->locals.count - 1; i >= 0; i--) {
    Local *local = &compiler->locals.items[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal,
                      bool readonly) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal)
      return i;
  }

  if (upvalueCount == UINT8_MAX) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  compiler->upvalues[upvalueCount].readonly = readonly;

  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == nullptr)
    return -1;

  int localIndex = resolveLocal(compiler->enclosing, name);
  if (localIndex != -1) {
    compiler->enclosing->locals.items[localIndex].captured = true;
    return addUpvalue(compiler, (uint8_t)localIndex, true,
                      compiler->enclosing->locals.items[localIndex].readonly);
  }

  int upvalueIndex = resolveUpvalue(compiler->enclosing, name);
  if (upvalueIndex != -1) {
    return addUpvalue(compiler, (uint8_t)upvalueIndex, false,
                      compiler->enclosing->locals.items[localIndex].readonly);
  }

  return -1;
}

static void addLocal(Token name, bool readonly) {
  Local local = {
      .name = name, .depth = -1, .captured = false, .readonly = readonly};
  writeLocalArray(&current->locals, local);
}

static void declareVariable(Token *name, bool readonly) {
  for (int i = current->locals.count - 1; i >= 0; i--) {
    Local *local = &current->locals.items[i];

    if (local->depth != -1 && local->depth < current->scopeDepth)
      break;

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name, readonly);
}

static VariableRef parseVariable(const char *errorMessage) {
  VariableRef ref;

  ref.readonly = parser.previous.type == TOKEN_LET;

  consume(TOKEN_IDENTIFIER, errorMessage);

  if (current->scopeDepth == 0) {
    ref.type = VAR_GLOBAL;
    ref.as.global = identifierConstant(&parser.previous);
  } else {
    ref.type = VAR_LOCAL;
    declareVariable(&parser.previous, ref.readonly);
  }

  return ref;
}

static void markInitialized(bool readonly) {
  if (current->scopeDepth == 0)
    return;

  Local *local = &current->locals.items[current->locals.count - 1];
  local->depth = current->scopeDepth;
  local->readonly = readonly;
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void defineVariable(VariableRef ref) {
  if (ref.type == VAR_LOCAL) {
    markInitialized(ref.readonly);
    return;
  }

  emitOpAndConstant(ref.as.global, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == UINT8_MAX) {
        error("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

  return argCount;
}

static void and_(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      VariableRef ref = parseVariable("Expect parameter name.");
      defineVariable(ref);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  if (type == TYPE_INITIALIZER && !check(TOKEN_LEFT_BRACE)) {
    if (currentClass->superclass == nullptr)
      errorAtCurrent("Can only call superclass init inside a derived class.");

    consume(TOKEN_COLON, "Expect ':' after init in derived class.");
    consume(TOKEN_SUPER,
            "Expect 'super' to call superclass init automatically.");
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'super'.");

    emitBytes(OP_GET_LOCAL, 0); // Instance is stored at slot 0

    uint8_t argCout = argumentList();

    ConstRef superclass = makeConstant(OBJ_VAL(currentClass->superclass));
    emitOpAndConstant(superclass, OP_GET_GLOBAL, OP_GET_GLOBAL_LONG);

    ConstRef ref = makeConstant(OBJ_VAL(vm.initString));
    emitOpAndConstant(ref, OP_SUPER_INVOKE, OP_SUPER_INVOKE_LONG);
    emitByte(argCout);
    emitByte(OP_POP);
  }

  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

  block();

  ObjFunction *fun = endCompiler();

  if (fun->upvalueCount > 0) {
    emitClosure(OBJ_VAL(fun));

    for (int i = 0; i < fun->upvalueCount; i++) {
      emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
      emitByte(compiler.upvalues[i].index);
    }
  } else {
    emitConstant(OBJ_VAL(fun));
  }
}

static void method() {
  consume(TOKEN_IDENTIFIER, "Expect method name.");

  if (parser.previous.length == vm.initString->length &&
      memcmp(parser.previous.start, getCString(vm.initString),
             vm.initString->length) == 0) {
    function(TYPE_INITIALIZER);
    emitByte(OP_INIT);
  } else {
    ConstRef ref = identifierConstant(&parser.previous);
    function(TYPE_METHOD);
    emitOpAndConstant(ref, OP_METHOD, OP_METHOD_LONG);
  }
}

static void variable(bool canAssign);
static void namedVariable(Token name, bool canAssign);

static Token syntheticToken(const char *text) {
  Token token;
  token.start = text;
  token.length = strlen(text);
  return token;
}

static void super_(bool canAssign) {
  if (currentClass == nullptr)
    error("Can't use 'super' outside of a class.");
  else if (currentClass->superclass == nullptr)
    error("Can't use 'super' in a class with no superclass.");

  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  ConstRef ref = identifierConstant(&parser.previous);

  namedVariable(syntheticToken("this"), false);

  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCout = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitOpAndConstant(ref, OP_SUPER_INVOKE, OP_SUPER_INVOKE_LONG);
    emitByte(argCout);
  } else {
    namedVariable(syntheticToken("super"), false);
    emitOpAndConstant(ref, OP_GET_SUPER, OP_GET_SUPER_LONG);
  }
}

static void classDeclaration() {
  Token className = parser.current;
  ConstRef nameConstant = identifierConstant(&className);
  VariableRef ref = parseVariable("Expected class name.");

  emitOpAndConstant(nameConstant, OP_CLASS, OP_CLASS_LONG);

  defineVariable(ref);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  classCompiler.superclass = nullptr;
  currentClass = &classCompiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);

    if (identifiersEqual(&className, &parser.previous))
      error("A class can't inherit from itself.");

    classCompiler.superclass =
        borrowString(parser.previous.start, parser.previous.length);

    beginScope();
    addLocal(syntheticToken("super"), true);
    VariableRef ref = {.type = VAR_LOCAL, .readonly = true};
    defineVariable(ref);

    namedVariable(className, false);
    emitByte(OP_INHERIT);
  }

  namedVariable(className, false);

  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    method();

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

  emitByte(OP_POP); // Pop the class from the stack

  if (classCompiler.superclass != nullptr)
    endScope();

  currentClass = currentClass->enclosing;
}

static void funDeclaration() {
  VariableRef global = parseVariable("Expect function name.");
  markInitialized(true);
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration() {
  VariableRef ref = parseVariable("Expect variable name");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    if (ref.readonly)
      error("Expect expression after 'let'");

    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaraion.");

  defineVariable(ref);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void continueStatement() {
  if (current->loopDepth == 0)
    error("Expect 'continue' inside loop statements only.");

  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

  // Pop all the locals that are in scopes deeper than the loop start depth
  for (int i = current->locals.count;
       i > 0 && current->locals.items[i - 1].depth >
                    current->loopReference[current->loopDepth - 1].scopeDepth;
       i--) {
    emitByte(OP_POP);
  }

  emitLoop(current->loopReference[current->loopDepth - 1].start);
}

static void switchStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  consume(TOKEN_LEFT_BRACE, "Expect '{' after switch condition.");

  int caseJumps[UINT8_MAX];
  uint8_t caseCount = 0;

  int nextCase = -1;
  while (match(TOKEN_CASE)) {
    if (nextCase != -1) {
      patchJump(nextCase);
      emitByte(OP_POP);
    }
    expression();
    consume(TOKEN_COLON, "Expect ':' after case expression");
    // Compare stack values, but unlike OP_EQUAL leave the first value on it
    emitByte(OP_CMP);
    nextCase = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT))
      statement();
    caseJumps[caseCount++] = emitJump(OP_JUMP);
  }
  if (nextCase != -1) {
    patchJump(nextCase);
    emitByte(OP_POP);
  }

  if (match(TOKEN_DEFAULT)) {
    consume(TOKEN_COLON, "Expect ':' after default");
    while (!check(TOKEN_RIGHT_BRACE))
      statement();
    caseJumps[caseCount++] = emitJump(OP_JUMP);
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch statement.");

  for (uint8_t i = 0; i < caseCount; i++) {
    patchJump(caseJumps[i]);
  }
}

static void forStatement() {
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // No initializer
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;

  int exitJump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    // Jump out of the loop if condition is false
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  beginLoop(loopStart);

  statement();

  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }

  endLoop();

  endScope();
}

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE))
    statement();
  patchJump(elseJump);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer.");
    }
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}

static void whileStatement() {
  int loopStart = currentChunk()->count;
  beginLoop(loopStart);

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);

  endLoop();
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;

    switch (parser.previous.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_LET:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:;
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_CLASS)) {
    classDeclaration();
  } else if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_VAR) || match(TOKEN_LET)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)rule->precedence + 1);

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  default: // Unreachable
    return;
  }
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static Value parseString(Token token) {
  if (token.length < 7) // STR max len is 4 but token includes the double quotes
    return SHORT_STRING_VAL(token.start + 1, token.length - 2);
  else
    return OBJ_VAL(borrowString(token.start + 1, token.length - 2));
}

static void string(bool canAssign) {
  emitConstant(parseString(parser.previous));
}

static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  ConstRef ref = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitOpAndConstant(ref, OP_SET_PROP, OP_SET_PROP_LONG);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitOpAndConstant(ref, OP_INVOKE, OP_INVOKE_LONG);
    emitByte(argCount);
  } else {
    emitOpAndConstant(ref, OP_GET_PROP, OP_GET_PROP_LONG);
  }
}

static void accessor(bool canAssign) {
  expression();

  consume(TOKEN_RIGHT_SQBRA, "Expect ']' after accessor expression.");

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitByte(OP_SET_PROP_STR);
  } else {
    emitByte(OP_GET_PROP_STR);
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  default: // Unreachable
    break;
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, nullptr);
  emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

static void localVariable(int index, bool canAssign) {
  if (canAssign && match(TOKEN_EQUAL)) {
    if (current->locals.items[index].readonly)
      error("Invalid assignment target: readonly variable.");
    expression();
    emitBytes(OP_SET_LOCAL, index);
  } else {
    emitBytes(OP_GET_LOCAL, index);
  }
}

static void upvalueVariable(int index, bool canAssign) {
  if (canAssign && match(TOKEN_EQUAL)) {
    if (current->upvalues[index].readonly)
      error("Invalid assignment target: readonly captured variable.");
    expression();
    emitBytes(OP_SET_UPVALUE, index);
  } else {
    emitBytes(OP_GET_UPVALUE, index);
  }
}

static void globalVariable(Token name, bool canAssign) {
  ConstRef ref = identifierConstant(&name);
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitOpAndConstant(ref, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG);
  } else {
    emitOpAndConstant(ref, OP_GET_GLOBAL, OP_GET_GLOBAL_LONG);
  }
}

static void namedVariable(Token name, bool canAssign) {
  int index = resolveLocal(current, &name);
  if (index != -1) {
    localVariable(index, canAssign);
    return;
  }

  index = resolveUpvalue(current, &name);
  if (index != -1) {
    upvalueVariable(index, !current->upvalues[index].readonly);
    return;
  }

  globalVariable(name, canAssign);
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void this_(bool canAssign) {
  if (currentClass == nullptr) {
    error("can't use 'this' outside of a class.");
    return;
  }

  variable(false);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  default:
    return;
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_LEFT_SQBRA] = {nullptr, accessor, PREC_CALL},
    [TOKEN_RIGHT_SQBRA] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_COMMA] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_DOT] = {nullptr, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {nullptr, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_SLASH] = {nullptr, binary, PREC_FACTOR},
    [TOKEN_STAR] = {nullptr, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, nullptr, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {nullptr, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {nullptr, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {nullptr, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {nullptr, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {nullptr, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {nullptr, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, nullptr, PREC_NONE},
    [TOKEN_STRING] = {string, nullptr, PREC_NONE},
    [TOKEN_NUMBER] = {number, nullptr, PREC_NONE},
    [TOKEN_AND] = {nullptr, and_, PREC_AND},
    [TOKEN_CLASS] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_ELSE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_FALSE] = {literal, nullptr, PREC_NONE},
    [TOKEN_FOR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_FUN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_IF] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_NIL] = {literal, nullptr, PREC_NONE},
    [TOKEN_OR] = {nullptr, or_, PREC_OR},
    [TOKEN_PRINT] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_RETURN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_SUPER] = {super_, nullptr, PREC_NONE},
    [TOKEN_THIS] = {this_, nullptr, PREC_NONE},
    [TOKEN_TRUE] = {literal, nullptr, PREC_NONE},
    [TOKEN_LET] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_VAR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_WHILE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_ERROR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_EOF] = {nullptr, nullptr, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == nullptr) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

ObjFunction *compile(const char *source) {
#ifdef DEBUG_PRINT_CODE
  debug("## COMPILATION TRACE START ##\n");
#endif

  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction *fun = endCompiler();

#ifdef DEBUG_PRINT_CODE
  debug("## COMPILATION TRACE END ##\n");
#endif

  return parser.hadError ? nullptr : fun;
}

void markCompilerRoots() {
  Compiler *compiler = current;
  while (compiler != nullptr) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}
