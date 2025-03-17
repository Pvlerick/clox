#include <err.h>
#include <stdio.h>
#include <stdlib.h>

char *readFile(const char *path) {
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
