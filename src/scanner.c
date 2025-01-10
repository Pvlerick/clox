#include "scanner.h"
#include <string.h>

typedef struct {
  const char *start;
  const char *current;
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

bool isAtEnd() { return *scanner.current == '\0'; }

Token makeToken(TokenType type) {
  Token token = {
      .type = type,
      .start = scanner.start,
      .length = (int)(scanner.current - scanner.start),
      .line = scanner.line,
  };

  return token;
}

Token errorToken(const char *message) {
  Token token = {
      .type = TOKEN_ERROR,
      .start = message,
      .length = (int)strlen(message),
      .line = scanner.line,
  };

  return token;
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static bool match(const char expected) {
  if (isAtEnd())
    return false;
  if (*scanner.current != expected)
    return false;

  scanner.current++;
  return true;
}

static char peek() { return *scanner.current; }

static char peekNext() {
  if (isAtEnd())
    return '\0';

  return scanner.current[1];
}

static void skipWhitespace() {
  for (;;) {
    switch (peek()) {
    case '\n':
      scanner.line++;
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '/':
      if (peekNext() == '/') {
        while (peek() != '\n' && !isAtEnd())
          advance();
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n')
      scanner.line++;
    advance();
  }

  if (isAtEnd())
    return errorToken("Unterminated string.");

  advance(); // Closing quote
  return makeToken(TOKEN_STRING);
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static Token number() {
  while (isDigit(peek()))
    advance();

  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek()))
      advance();
  }

  return makeToken(TOKEN_NUMBER);
}

static bool isAlpha(char c) {
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static TokenType checkKeyword(int start, int length, const char *rest,
                              TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp((void *)(scanner.start + start), rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

TokenType identifierType() {
  switch (scanner.start[0]) {
  case 'a':
    return checkKeyword(1, 2, "nd", TOKEN_AND);
  case 'c':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 2, "se", TOKEN_CASE);
      case 'l':
        return checkKeyword(2, 3, "ass", TOKEN_CLASS);
      case 'o':
        return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
      }
    }
  case 'd':
    return checkKeyword(1, 6, "efault", TOKEN_DEFAULT);
  case 'e':
    return checkKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'f':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(2, 1, "n", TOKEN_FUN);
      }
    }
  case 'i':
    return checkKeyword(1, 1, "f", TOKEN_IF);
  case 'l':
    return checkKeyword(1, 2, "et", TOKEN_LET);
  case 'n':
    return checkKeyword(1, 2, "il", TOKEN_NIL);
  case 'o':
    return checkKeyword(1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'w':
        return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
      case 'u':
        return checkKeyword(2, 3, "per", TOKEN_SUPER);
      }
    }
  case 't':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return checkKeyword(2, 2, "is", TOKEN_THIS);
      case 'r':
        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
      }
    }
  case 'v':
    return checkKeyword(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

Token identifier() {
  while (isAlpha(peek()) && !isAtEnd())
    advance();

  return makeToken(identifierType());
}

Token scanToken() {
  skipWhitespace();

  scanner.start = scanner.current;

  if (isAtEnd())
    return makeToken(TOKEN_EOF);

  char c = advance();

  if (isAlpha(c))
    return identifier();

  if (isDigit(c))
    return number();

  switch (c) {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE);
  case ';':
    return makeToken(TOKEN_SEMICOLON);
  case ':':
    return makeToken(TOKEN_COLON);
  case ',':
    return makeToken(TOKEN_COMMA);
  case '.':
    return makeToken(TOKEN_DOT);
  case '-':
    return makeToken(TOKEN_MINUS);
  case '+':
    return makeToken(TOKEN_PLUS);
  case '/':
    return makeToken(TOKEN_SLASH);
  case '*':
    return makeToken(TOKEN_STAR);
  case '!':
    return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"':
    return string();
  }

  return errorToken("Unexpected character.");
}
