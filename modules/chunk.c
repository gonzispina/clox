//
// Created by gonzalo on 3/11/21.
//

#include <stdlib.h>

#include "chunk.h"
#include "memory.h"


void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initTable(&chunk->globals);
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeTable(&chunk->globals);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count-1;
}

int addIdentifier(Chunk *chunk, Value name) {
    ObjString* key = AS_STRING(name);
    Value value;
    if (tableGet(&chunk->globals, key, &value)) {
        return (int)AS_NUMBER(value);
    }

    int ptr = addConstant(chunk, name);
    tableSet(&chunk->globals, key, NUMBER_VAL(ptr));

    return ptr;
}
