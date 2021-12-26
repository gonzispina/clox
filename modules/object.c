//
// Created by gonzalo on 12/12/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "table.h"

Obj* allocateObj(Obj* prev, ObjType type, size_t size) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    if (prev != NULL) {
        obj->next = prev;
        prev = obj;
    }
    return obj;
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_FUNCTION: {
            ObjFunction* function = AS_FUNCTION(value);
            if (function->name != NULL) printf("<fn %s>", function->name->chars);
            else printf("<script>");
            break;
        }
    }
}

ObjFunction *newFunction() {
    ObjFunction* function = (ObjFunction*)allocateObj(NULL, OBJ_FUNCTION, sizeof(ObjFunction));
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}
