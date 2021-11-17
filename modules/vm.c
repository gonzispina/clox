//
// Created by gonzalo on 17/11/21.
//

#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "value.h"

static void resetStack(Stack* stack) {
    stack->top = stack->values;
}

void push(Stack *stack, Value value) {
    *stack->top = value;
    stack->top++;
}

Value pop(Stack *stack) {
    stack->top--;
    return *stack->top;
}

VM initVM() {
    VM vm;
    resetStack(&vm.stack);
    return vm;
}

void freeVM(VM* vm) {

}


static uint8_t READ_BYTE(VM* vm) {
    return *vm->ip++;
}

static Value READ_CONSTANT(VM* vm) {
    return vm->chunk->constants.values[READ_BYTE(vm)];
}

static void printStack(Stack* stack) {
    printf("          ");
    for (Value* slot = stack->values; slot < stack->top; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }
    printf("\n");
}

static InterpretResult run(VM* vm) {
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printStack(&vm->stack);
    disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE(vm)) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT(vm);
                push(&vm->stack, constant);
                printValue(constant);
                printf("\n");
                break;
            }
            case OP_RETURN: {
                printValue(pop(&vm->stack));
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
    return run(vm);
}


