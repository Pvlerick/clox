#include "assembly.h"
#include "object.h"
#include "utils.h"
#include <err.h>
#include <stdio.h>
#include <string.h>

static int writeLines(FILE *file, LineArray *lines) {
  if (fwrite(&lines->count - 1, sizeof(uint32_t), 1, file) != 1)
    return -1;

  if (fwrite(&lines->items, sizeof(LineItem), lines->count - 1, file) !=
      lines->count - 1)
    return -1;

  return 0;
}

static int writeConstants(FILE *file, ValueArray *constants) {
  uint32_t constCount = constants->count - 1;
  if (fwrite(&constCount, sizeof(uint32_t), 1, file) != 1)
    return -1;

  for (int i = 0; i < constants->count - 1; i++) {
    Value value = constants->values[i];
    if (fputc(value.type, file) == EOF)
      return -1;
    switch (value.type) {
    case VAL_NUMBER:
      if (fwrite(&value.as.number, sizeof(double), 1, file) != 1)
        return -1;
      break;
    case VAL_BOOL:
      if (fwrite(&value.as.boolean, sizeof(bool), 1, file) != 1)
        return -1;
      break;
    case VAL_OBJ:
      switch (value.as.obj->type) {
      case OBJ_STRING:
        ObjString *str = AS_STRING(value);
        printf("str: %.*s\n", str->length, getCString(str));
        if (fwrite(getCString(str), str->length, 1, file) != 1)
          return -1;
        break;
      case OBJ_FUNCTION:
        ObjFunction *fun = AS_FUNCTION(value);
        printf("fun: %.*s\n", fun->name->length, getCString(fun->name));
        break;
      default:
        err(1, "Obj type not supported as constant: %d", value.as.obj->type);
        break;
      }
    default:
      break;
    }
  }

  return 0;
}

void writeAssembly(ObjFunction *entrypoint, const char *path) {
  FILE *file = nullptr;

  file = fopen(path, "w");
  if (file == nullptr) {
    err(74, "Cannot open file '%s'", path);
  }

  const char *HEADER = "CLASM";
  if (fwrite(HEADER, strlen(HEADER), 1, file) != 1)
    goto error;

  uint32_t codeLength = entrypoint->chunk.count - 1;
  if (fwrite(&codeLength, sizeof(uint32_t), 1, file) != 1)
    goto error;

  if (fwrite(entrypoint->chunk.code, sizeof(uint8_t),
             entrypoint->chunk.count - 1, file) != entrypoint->chunk.count - 1)
    goto error;

  if (writeLines(file, &entrypoint->chunk.lines) == -1)
    goto error;

  if (writeConstants(file, &entrypoint->chunk.constants) == -1)
    goto error;

  if (fclose(file) != 0) {
    err(74, "Cannot close file '%s'", path);
  }

  return;

error:
  if (fclose(file) != 0) {
    err(74, "Cannot close file '%s' after failing to write to it", path);
  }
  err(74, "Cannot write to file '%s'", path);
}

ObjFunction *readAssembly(const char *path) {
  char *content = readFile(path);

  if (strncmp(content, "CLASM", 5) != 0)
    err(1, "CLOX assembly header invalid in file '%s'.", path);

  content = content + 5;

  uint32_t codeLength = (uint32_t)content[0] | (uint32_t)content[1] << 8 |
                        (uint32_t)content[2] << 16 | (uint32_t)content[3] << 24;

  printf("code length: %d\n", codeLength);

  return nullptr;
}
