//
// Created by gonzalo on 12/12/21.
//

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

static Obj* allocateObj(Obj** prev, ObjType type, size_t size) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    if (prev != NULL) {
        obj->next = *(prev);
        *(prev) = obj;
    }
    return obj;
}

ObjString* allocateString(Obj** prev, const char* chars, int length) {
    ObjString* str = (ObjString*)allocateObj(prev, OBJ_STRING, sizeof (ObjString)+length+1);
    str->length = length;
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    return str;
}

ObjString* copyString(const char* chars, int length) {
    return allocateString(NULL, chars, length);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
