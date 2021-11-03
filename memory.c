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

