//
// Created by gonzalo on 20/12/21.
//

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "strings.h"

ObjString* allocateString(VM* vm, const char* chars, int length, uint32_t hash) {
    ObjString* str = (ObjString*)allocateObj(vm->objects, OBJ_STRING, sizeof (ObjString)+length+1);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->length = length;
    str->hash = hash;
    tableSet(&vm->strings, str, NIL_VAL);
    return str;
}

ObjString* copyString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}

ObjString* takeString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}