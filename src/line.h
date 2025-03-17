#ifndef clox_line_h
#define clox_line_h

#include <stdint.h>
typedef struct {
  uint32_t line;
  uint32_t offsetStart;
  uint32_t offsetEnd;
} LineItem; 

typedef struct {
  uint32_t count;
  uint32_t capacity;
  LineItem* items;
} LineArray;

void initLineArray(LineArray *array);
void freeLineArray(LineArray *array);
void writeLineArray(LineArray *array, LineItem *item);
void addInstructionLine(LineArray *array, int offset, int line);
int getInstructionLine(LineArray *array, int offset);

#endif
