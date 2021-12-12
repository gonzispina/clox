//
// Created by gonzalo on 3/11/21.
//

#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* res = realloc(pointer, newSize);
    if (res == NULL) exit(0);

    return res;
}

static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            free(object);
            break;
        }
    }
}

void freeObjects(Obj* object) {
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}

