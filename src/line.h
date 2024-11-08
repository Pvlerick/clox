#ifndef clox_line_h
#define clox_line_h

typedef struct {
  int line;
  int offsetStart;
  int offsetEnd;
} LineItem; 

typedef struct {
  int count;
  int capacity;
  LineItem* items;
} LineArray;

void initLineArray(LineArray *array);
void freeLineArray(LineArray *array);
void writeLineArray(LineArray *array, LineItem *item);
void addInstructionLine(LineArray *array, int offset, int line);
int getInstructionLine(LineArray *array, int offset);

#endif
