//
// Created by gonzalo on 12/12/21.
//

#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"
#include "vm.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)
#define IS_STRING(value)     isObjType(value, OBJ_STRING)

#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
};

ObjString* copyString(const char* chars, int length);
ObjString* allocateString(Obj** prev, char* heapChars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif //CLOX_OBJECT_H
