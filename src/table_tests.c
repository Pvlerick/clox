#include "object.h"
#include "table.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjString *newString(const char *start) {
  size_t length = strlen(start);
  ObjString *string = (ObjString *)malloc(sizeof(ObjString) + length);

  string->length = length;
  string->isBorrowed = false;
  string->hash = hashString(start, length);
  memcpy((void *)string->content, (void *)start, length);

  return string;
}

static void set(Table *table, char *key, Value value) {
  tableSet(table, newString(key), value);
}

static void unset(Table *table, char *key) {
  tableDelete(table, newString(key));
}

static bool get(Table *table, char *key, Value *value) {
  return tableGet(table, newString(key), value);
}

int main() {
  Table table;
  initTable(&table);

  assert(table.count == 0);
  assert(table.capacity == 0);
  assert(table.entries == nullptr);

  set(&table, "foo", BOOL_VAL(true));

  assert(table.count == 1);
  assert(table.capacity == 8);
  assert(table.entries != nullptr);

  set(&table, "bar", BOOL_VAL(false));
  set(&table, "baz", BOOL_VAL(true));
  set(&table, "bax", BOOL_VAL(true));
  set(&table, "qux_1", BOOL_VAL(true));
  set(&table, "qux_2", BOOL_VAL(true));

  assert(table.count == 6);
  assert(table.capacity == 8);
  assert(table.entries != nullptr);

  set(&table, "qux_3", BOOL_VAL(true));

  assert(table.count == 7);
  assert(table.capacity == 16);
  assert(table.entries != nullptr);

  unset(&table, "baz");

  assert(table.count == 7);
  assert(table.capacity == 16);
  assert(table.entries != nullptr);

  Value value;
  assert(!get(&table, "baz", &value));

  unset(&table, "bar");
  unset(&table, "foo");
  unset(&table, "bax");
  unset(&table, "qux_1");

  assert(table.count == 7);
  assert(table.capacity == 16);
  assert(table.entries != nullptr);

  tableDump(&table);

  freeTable(&table);

  printf("All tests passed!\n");

  return EXIT_SUCCESS;
}
