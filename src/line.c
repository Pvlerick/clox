#include <stdio.h>
#include <stdlib.h>

#include "line.h"
#include "memory.h"

void addInstructionLine(LineArray *array, int offset, int line) {
  if (array->count > 0 && array->items[array->count - 1].line == line) {
    array->items[array->count - 1].offsetEnd = offset;
  } else {
    LineItem item;
    item.line = line;
    item.offsetStart = offset;
    item.offsetEnd = offset;
    writeLineArray(array, &item);
  }
}

int getInstructionLine(LineArray *array, int offset) {
  for (int i = 0; i < array->count; i++) {
    LineItem *item = &array->items[i];
    if (item->offsetStart <= offset && item->offsetEnd >= offset) {
      return item->line;
    }
  }
  // Line not found for this instruction; bail
  exit(1);
}

void initLineArray(LineArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->items = NULL;
}

void freeLineArray(LineArray *array) {
  FREE_ARRAY(LineItem, array->items, array->capacity);
  initLineArray(array);
}

void writeLineArray(LineArray *array, LineItem *item) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->items =
        GROW_ARRAY(LineItem, array->items, oldCapacity, array->capacity);
  }

  array->items[array->count] = *item;
  array->count++;
}
