#ifndef clox_assembly_h
#define clox_assembly_h

#include "value.h"

void writeAssembly(ObjFunction *entrypoint, const char *path);
ObjFunction *readAssembly(const char *path);

#endif

