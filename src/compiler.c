#include "chunk.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>

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

typedef void (*ParseFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static int depth = 0;
static void printDepth() {
  for (int i = 0; i < depth; i++)
    printf(" ");
}
static void incDepth() { depth++; }
static void decDepth() { depth--; }

static const char *printableTokenType(TokenType type) {
  switch (type) {

  case TOKEN_LEFT_PAREN:
    return "TOKEN_LEFT_PAREN";
  case TOKEN_RIGHT_PAREN:
    return "TOKEN_RIGHT_PAREN";
  case TOKEN_LEFT_BRACE:
    return "TOKEN_LEFT_BRACE";
  case TOKEN_RIGHT_BRACE:
    return "TOKEN_RIGHT_BRACE";
  case TOKEN_COMMA:
    return "TOKEN_COMMA";
  case TOKEN_DOT:
    return "TOKEN_DOT";
  case TOKEN_MINUS:
    return "TOKEN_MINUS";
  case TOKEN_PLUS:
    return "TOKEN_PLUS";
  case TOKEN_SEMICOLON:
    return "TOKEN_SEMICOLON";
  case TOKEN_SLASH:
    return "TOKEN_SLASH";
  case TOKEN_STAR:
    return "TOKEN_STAR";
  case TOKEN_BANG:
    return "TOKEN_BANG";
  case TOKEN_BANG_EQUAL:
    return "TOKEN_BANG_EQUAL";
  case TOKEN_EQUAL:
    return "TOKEN_EQUAL";
  case TOKEN_EQUAL_EQUAL:
    return "TOKEN_EQUAL_EQUAL";
  case TOKEN_GREATER:
    return "TOKEN_GREATER";
  case TOKEN_GREATER_EQUAL:
    return "TOKEN_GREATER_EQUAL";
  case TOKEN_LESS:
    return "TOKEN_LESS";
  case TOKEN_LESS_EQUAL:
    return "TOKEN_LESS_EQUAL";
  case TOKEN_IDENTIFIER:
    return "TOKEN_IDENTIFIER";
  case TOKEN_STRING:
    return "TOKEN_STRING";
  case TOKEN_NUMBER:
    return "TOKEN_NUMBER";
  case TOKEN_AND:
    return "TOKEN_AND";
  case TOKEN_CLASS:
    return "TOKEN_CLASS";
  case TOKEN_ELSE:
    return "TOKEN_ELSE";
  case TOKEN_FALSE:
    return "TOKEN_FALSE";
  case TOKEN_FOR:
    return "TOKEN_FOR";
  case TOKEN_FUN:
    return "TOKEN_FUN";
  case TOKEN_IF:
    return "TOKEN_IF";
  case TOKEN_NIL:
    return "TOKEN_NIL";
  case TOKEN_OR:
    return "TOKEN_OR";
  case TOKEN_PRINT:
    return "TOKEN_PRINT";
  case TOKEN_RETURN:
    return "TOKEN_RETURN";
  case TOKEN_SUPER:
    return "TOKEN_SUPER";
  case TOKEN_THIS:
    return "TOKEN_THIS";
  case TOKEN_TRUE:
    return "TOKEN_TRUE";
  case TOKEN_VAR:
    return "TOKEN_VAR";
  case TOKEN_WHILE:
    return "TOKEN_WHILE";
  case TOKEN_ERROR:
    return "TOKEN_ERROR";
  case TOKEN_EOF:
    return "TOKEN_EOF";
  }
}

static Chunk *currentChunk() { return compilingChunk; }

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

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitReturn() { emitByte(OP_RETURN); }

static void emitConstant(Value value) {
  writeConstant(currentChunk(), value, parser.previous.line);
}

static void endCompiler() {
  emitReturn();

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static ParseRule *getRule(TokenType);
static void parsePrecedence(Precedence);

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void binary() {
  printf("handler: %s\n", __FUNCTION__);

  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)rule->precedence + 1);

  switch (operatorType) {
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

static void grouping() {
  printf("handler: %s\n ", __FUNCTION__);
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
  printf("handler: %s\n", __FUNCTION__);
  double value = strtod(parser.previous.start, nullptr);
  emitConstant(value);
}

static void unary() {
  printf("handler: %s\n", __FUNCTION__);

  TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return;
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, nullptr, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_COMMA] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_DOT] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {nullptr, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_SLASH] = {nullptr, binary, PREC_FACTOR},
    [TOKEN_STAR] = {nullptr, binary, PREC_FACTOR},
    [TOKEN_BANG] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_GREATER] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_LESS] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_IDENTIFIER] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_STRING] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_NUMBER] = {number, nullptr, PREC_NONE},
    [TOKEN_AND] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_CLASS] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_ELSE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_FALSE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_FOR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_FUN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_IF] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_NIL] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_OR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_PRINT] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_RETURN] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_SUPER] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_THIS] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_TRUE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_VAR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_WHILE] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_ERROR] = {nullptr, nullptr, PREC_NONE},
    [TOKEN_EOF] = {nullptr, nullptr, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();

  printDepth();
  printf("token: %s | precedence: %d | ",
         printableTokenType(parser.previous.type), precedence);

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == nullptr) {
    error("Expect expression.");
    return;
  }

  incDepth();
  prefixRule();
  decDepth();

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();

    printDepth();
    printf("token: %s | precedence: %d | ",
           printableTokenType(parser.previous.type), precedence);

    ParseFn infixfRule = getRule(parser.previous.type)->infix;

    incDepth();
    infixfRule();
    decDepth();
  }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

bool compile(const char *source, Chunk *chunk) {
  initScanner(source);
  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance();
  expression();

  consume(TOKEN_EOF, "Expect end of expression.");

  endCompiler();
  return !parser.hadError;
}
