//
// Created by gonzalo on 17/11/21.
//

#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "common.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Value values[STACK_MAX];
    Value* top;
} Stack;

typedef struct VM {
    Chunk* chunk;
    uint8_t* ip; // Instruction pointer
    Stack stack;
} VM;

void push(Stack* stack, Value value);

Value pop(Stack* stack);

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

VM* initVM();
void freeVM(VM*);

InterpretResult interpret(VM* vm, const char* source);

#endif //CLOX_VM_H