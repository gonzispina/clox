//
// Created by gonzalo on 17/11/21.
//

#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "value.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

void push(Value value) {
    *(vm.stackTop) = value;
    vm.stackTop += 1;
}

Value pop() {
    vm.stackTop -= 1;
    return *(vm.stackTop);
}

void initVM() {
    resetStack();
}

void freeVM() {

}


static uint8_t READ_BYTE() {
    return *vm.ip++;
}

static Value READ_CONSTANT() {
    return vm.chunk->constants.values[READ_BYTE()];
}

static void printStack() {
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }
    printf("\n");
}

static InterpretResult run() {
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printStack();
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE(vm)) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT(vm);
                push(constant);
                printValue(constant);
                printf("\n");
                break;
            }
            case OP_NEGATE: {
                push(-pop());
                break;
            }
            case OP_RETURN: {
                printValue(pop());
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = chunk->code;
    return run();
}


