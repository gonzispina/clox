//
// Created by gonzalo on 17/11/21.
//

#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip; // Instruction pointer
    Value stack[STACK_MAX];
    Value* stackTop;
} VM;

void push(Value value);

Value pop();


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

#endif //CLOX_VM_H
