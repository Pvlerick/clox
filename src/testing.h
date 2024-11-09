#ifndef clox_testing_h
#define clox_testing_h

#include <stdlib.h>

#define ASSERT_EQ_SIZET(EXPECTED, ACTUAL) \
  if ((EXPECTED) != (ACTUAL)) { \
    fprintf(stderr, "%s, line %d: for '%s', expected %zu, got %zu\n", __FILE__, __LINE__, #ACTUAL, EXPECTED, ACTUAL); \
    exit(1); \
  }

#define ASSERT_EQ_PTR(EXPECTED, ACTUAL) \
  if ((EXPECTED) != (ACTUAL)) { \
    fprintf(stderr, "%s, line %d: for '%s', expected %p, got %p\n", __FILE__, __LINE__, #ACTUAL, EXPECTED, ACTUAL); \
    exit(1); \
  }

#define ASSERT_NE_PTR(EXPECTED, ACTUAL) \
  if ((EXPECTED) == (ACTUAL)) { \
    fprintf(stderr, "%s, line %d: for '%s', expected %p, got %p\n", __FILE__, __LINE__, #ACTUAL, EXPECTED, ACTUAL); \
    exit(1); \
  }

#endif
