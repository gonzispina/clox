//
// Created by gonzalo on 17/11/21.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "value.h"
#include "compiler.h"

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

Value peek(Stack* stack, int distance) {
    return *(stack->top - distance);
}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack(&vm->stack);
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

static bool validateNumbers(VM* vm, Value a, Value b) {
    if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
        runtimeError(vm, "Operands must be a numbers.");
        return false;
    }
    return true;
}

typedef void (*BinaryFn)(VM* vm, Value a, Value b);

static void sumOp(VM* vm, Value a, Value b) {
    push(&vm->stack, NUMBER_VAL(AS_NUMBER(a)+AS_NUMBER(b)));
}

static void substractOp(VM* vm, Value a, Value b) {
    push(&vm->stack, NUMBER_VAL(AS_NUMBER(a)-AS_NUMBER(b)));
}

static void multiplyOp(VM* vm, Value a, Value b) {
    push(&vm->stack, NUMBER_VAL(AS_NUMBER(a)*AS_NUMBER(b)));
}

static void divideOp(VM* vm, Value a, Value b) {
    push(&vm->stack, NUMBER_VAL(AS_NUMBER(a)/AS_NUMBER(b)));
}

static bool binaryOp(VM* vm, ValueType type, BinaryFn op) {
    Value b = pop(&vm->stack);
    Value a = pop(&vm->stack);
    if (type == VAL_NUMBER && !validateNumbers(vm, a, b)) return false;
    op(vm, a, b);
    return true;
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
            case OP_NIL: push(&vm->stack, NIL_VAL); break;
            case OP_TRUE: push(&vm->stack, BOOL_VAL(true)); break;
            case OP_FALSE: push(&vm->stack, BOOL_VAL(false)); break;
            case OP_ADD: if (!binaryOp(vm, VAL_NUMBER, sumOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_SUBTRACT: if (!binaryOp(vm, VAL_NUMBER, substractOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_MULTIPLY: if (!binaryOp(vm, VAL_NUMBER, multiplyOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_DIVIDE: if (!binaryOp(vm, VAL_NUMBER, divideOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(&vm->stack, 0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(&vm->stack, NUMBER_VAL(-AS_NUMBER(pop(&vm->stack))));
                break;
            }
            case OP_RETURN: {
                printValue(pop(&vm->stack));
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(VM* vm, const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult res = run(vm);
    freeChunk(&chunk);

    return res;
}


