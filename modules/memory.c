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
            free(object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            if (function->name != NULL) free(function->name);
            free(function);
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

