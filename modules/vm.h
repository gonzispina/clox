//
// Created by gonzalo on 17/11/21.
//

#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct Stack {
    Value values[STACK_MAX];
    Value* top;
} Stack;

typedef struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Stack stack;
    Obj* objects;
    Table strings;
    Table globals;
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
