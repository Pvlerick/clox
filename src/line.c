#include <stdlib.h>

#include "line.h"
#include "memory.h"

void addInstructionLine(LineArray *array, int offset, int line) {}

int getInstructionLine(LineArray *array, int offset) { return 0; }
// for (int i = 0; i < chunk->lines.count; i++) {
//   if (instructionIndex <= chunk->lines.items[i].start &&
//       instructionIndex >= chunk->lines.items[i].end) {
//     return i;
//   }
// }
// // Line not found for this instruct; bail
// exit(1);

void initLineArray(LineArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->items = NULL;
}

void freeLineArray(LineArray *array) {
  FREE_ARRAY(LineItem, array->items, array->capacity);
  initLineArray(array);
}

void writeLineArray(LineArray *array, LineItem item) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->items =
        GROW_ARRAY(LineItem, array->items, oldCapacity, array->capacity);
  }

  array->items[array->count] = item;
  array->count++;
}
