//
// Created by gonzalo on 17/11/21.
//

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "value.h"

static void resetStack(Stack* stack) {
    stack->top = stack->values;
}

void push(Stack* stack, Value value) {
    *(stack->top) = value;
    stack->top++;
}

Value pop(Stack* stack) {
    stack->top--;
    return *(stack->top);
}

VM* initVM() {
    VM* vm = (VM*)malloc(sizeof(VM));
    if (vm == NULL) {
        exit(1);
    }

    resetStack(&vm->stack);
    return vm;
}

void freeVM(VM* vm) {
    free(vm);
}

static uint8_t READ_BYTE(VM* vm) {
    return *vm->ip++;
}

static Value READ_CONSTANT(VM* vm) {
    return vm->chunk->constants.values[READ_BYTE(vm)];
}

static void printStack(Stack* stack) {
    printf("          ");
    for (Value* slot = stack->top; slot < stack->top; slot++) {
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
            case OP_ADD: {
                double b = pop(&vm->stack);
                double a = pop(&vm->stack);
                push(&vm->stack, a+b);
                break;
            }
            case OP_SUBTRACT: {
                double b = pop(&vm->stack);
                double a = pop(&vm->stack);
                push(&vm->stack, a-b);
                break;
            }
            case OP_MULTIPLY: {
                double b = pop(&vm->stack);
                double a = pop(&vm->stack);
                push(&vm->stack, a*b);
                break;
            }
            case OP_DIVIDE: {
                double b = pop(&vm->stack);
                double a = pop(&vm->stack);
                push(&vm->stack, a/b);
                break;
            }
            case OP_NEGATE: push(&vm->stack, -pop(&vm->stack)); break;
            case OP_RETURN: {
                printValue(pop(&vm->stack));
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(VM* vm, const char* source) {
    compile(source);
    return run(vm);
}


