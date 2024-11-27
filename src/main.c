#include "vm.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

static void repl() {
  char line[1024];

  for (;;) {
    printf("> ");

    if (fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
  }

  interpret(line);
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    err(74, "Cannot open file \"%s\"", path);
  }

  if (fseek(file, 0L, SEEK_END) != 0) {
    if (fclose(file) != 0) {
      err(74, "Cannot close file '%s' after failing to seek to its end", path);
    }
    err(74, "Cannot seek to end of file '%s'", path);
  }

  size_t fileSize = ftell(file);

  if (fseek(file, 0L, SEEK_SET) != 0) {
    if (fclose(file) != 0) {
      err(74, "Cannot close file '%s' after failing to seek to its beginning",
          path);
    }
    err(74, "Cannot seek to beginning of file '%s'", path);
  }

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == nullptr) {
    err(74, "Not enough memory to read file \"%s\"", path);
  }

  size_t bytesRead;
  if ((bytesRead = fread(buffer, sizeof(char), fileSize, file)) < fileSize) {
    if (feof(file) != 0) {
      fprintf(stderr, "EOF encountered while reading file '%s'\n", path);
    } else if (ferror(file) != 0) {
      fprintf(stderr, "Error encountered while reading file '%s'\n", path);
    }

    if (fclose(file) != 0) {
      err(74, "Cannot close file '%s' after failing to read its content", path);
    }
    err(74, "Failed to read file content");
  }

  buffer[bytesRead] = '\0';

  if (fclose(file) != 0) {
    err(74, "Cannot close file '%s' after reading its content", path);
  }

  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  switch (result) {
  case INTERPRET_COMPILE_ERROR:
    exit(65);
  case INTERPRET_RUNTIME_ERROR:
    exit(70);
  case INTERPRET_OK:
    break;
  }
}

int main(int argc, const char *argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVM();

  return EXIT_SUCCESS;
}
