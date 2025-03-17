#include "utils.h"
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
