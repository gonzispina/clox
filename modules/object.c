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

ObjString* allocateString(Obj** prev, char* heapChars, int length) {
    ObjString* str = (ObjString*)allocateObj(prev, OBJ_STRING, sizeof (ObjString));
    str->length = length;
    str->chars = heapChars;
    return str;
}

ObjString* copyString(const char* chars, int length) {
    char* heapChars = (char*)reallocate(NULL, 0, sizeof(char) * (length + 1));
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(NULL, heapChars, length);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
