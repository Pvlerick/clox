#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdio.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = nullptr;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
  uint32_t index = key->hash & (capacity - 1);
  Entry *tombstone = nullptr;

  for (;;) {
    Entry *entry = &entries[index];

    if (entry->key == nullptr) {
      if (IS_NIL(entry->value)) // Empty entry
        return tombstone != nullptr ? tombstone : entry;
      else if (tombstone == nullptr) // Tombstone
        tombstone = entry;
    } else if (entry->key->hash == key->hash) { // Found it
      return entry;
    }

    index = (index + 1) & (capacity - 1);
  }
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);

  if (entry == nullptr)
    return false;

  if (entry->key == nullptr)
    return false;

  *value = entry->value;

  return true;
}

void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);

  for (int i = 0; i < capacity; i++) {
    entries[i].key = nullptr;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key == nullptr)
      continue;
    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == nullptr;
  if (isNewKey && IS_NIL(entry->value))
    table->count++;

  entry->key = key;
  entry->value = value;

  return isNewKey;
}

bool tableDelete(Table *table, ObjString *key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == nullptr)
    return false;

  entry->key = nullptr;
  entry->value = BOOL_VAL(true); // Tombstone
  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != nullptr)
      tableSet(to, entry->key, entry->value);
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
  if (table->count == 0)
    return nullptr;

  uint32_t index = hash & (table->capacity - 1);

  for (;;) {
    Entry *entry = &table->entries[index];
    if (entry->key == nullptr) {
      if (IS_NIL(entry->value))
        return nullptr;
    } else {
      if (entry->key->length == length && entry->key->hash == hash &&
          memcmp(getCString(entry->key), chars, length) == 0)
        return entry->key;
    }

    index = (index + 1) & (table->capacity - 1);
  }
}

void tableRemoveWhite(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key != nullptr && !IS_MARKED(&entry->key->obj)) {
      tableDelete(table, entry->key);
    }
  }
}

void markTable(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    Obj *key = (Obj *)entry->key;
    markObject((Obj *)entry->key);
    markValue(entry->value);
  }
}

void tableDump(Table *table) {
  printf("dumping table (count: %d, capacity: %d)\n", table->count,
         table->capacity);
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key != nullptr) {
      printf("[%d | '%.*s' | %d] -> ", i, entry->key->length,
             getCString(entry->key), entry->key->hash);
      printValue(entry->value);
      printf("\n");
    } else if (IS_BOOL(entry->value) && AS_BOOL(entry->value) == true) {
      printf("[%d | <tombstone> ]\n", i);
    } else {
      printf("[%d | <empty> ]\n", i);
    }
  }
}
