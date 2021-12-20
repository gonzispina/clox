//
// Created by gonzalo on 17/11/21.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "common.h"
#include "debug.h"
#include "vm.h"
#include "value.h"
#include "compiler.h"
#include "table.h"
#include "strings.h"

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

static void freeStack(Stack stack) {

}

static bool isFalsy(Value v) {
    return IS_NIL(v) || (IS_BOOL(v) && !AS_BOOL(v));
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
    initTable(&vm->strings);
    initTable(&vm->globals);
    vm->objects = NULL;
    return vm;
}

void freeVM(VM* vm) {
    freeObjects(vm->objects);
    freeTable(&vm->strings);
    freeTable(&vm->globals);
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

static bool validateEqualType(VM* vm, Value a, Value b) {
    return a.type == b.type;
}

typedef void (*BinaryFn)(VM* vm, Value a, Value b);

static void equalOp(VM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_BOOL:   push(&vm->stack, BOOL_VAL(AS_BOOL(a) == AS_BOOL(b))); break;
        case VAL_NIL:    push(&vm->stack, BOOL_VAL(true)); break;
        case VAL_NUMBER: push(&vm->stack, BOOL_VAL(AS_NUMBER(a) == AS_NUMBER(b))); break;
        case VAL_OBJ:    push(&vm->stack, BOOL_VAL(AS_OBJ(a) == AS_OBJ(b))); break;
        default: return; // Unreachable.
    }
}

static void greaterOp(VM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_BOOL:   push(&vm->stack, BOOL_VAL(AS_BOOL(a) > AS_BOOL(b))); break;
        case VAL_NIL:    push(&vm->stack, BOOL_VAL(false)); break;
        case VAL_NUMBER: push(&vm->stack, BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b))); break;
        default:         false; // Unreachable.
    }
}

static void lesserOp(VM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_BOOL:   push(&vm->stack, BOOL_VAL(AS_BOOL(a) < AS_BOOL(b))); break;
        case VAL_NIL:    push(&vm->stack, BOOL_VAL(false)); break;
        case VAL_NUMBER: push(&vm->stack, BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b))); break;
        default:         false; // Unreachable.
    }
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
    if (type == VAL_BOOL && !validateEqualType(vm, a, b)) return false;
    op(vm, a, b);
    return true;
}

static InterpretResult run(VM* vm) {
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    // printStack(&vm->stack);
    // disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
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
            case OP_EQUAL: if (!binaryOp(vm, VAL_BOOL, equalOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_GREATER: if (!binaryOp(vm, VAL_BOOL, greaterOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_LESSER: if (!binaryOp(vm, VAL_BOOL, lesserOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_ADD: {
                Value b = pop(&vm->stack);
                Value a = pop(&vm->stack);
                if (IS_STRING(a) && IS_STRING(b)) {
                    ObjString* strA = AS_STRING(a);
                    ObjString* strB =  AS_STRING(b);

                    int length = strA->length + strB->length;
                    char* concat = (char*)malloc(length);
                    memcpy(concat, strA->chars, strA->length);
                    memcpy(concat+strA->length, strB->chars, strB->length);

                    push(&vm->stack, OBJ_VAL(takeString(vm, concat, length)));
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    push(&vm->stack, NUMBER_VAL(AS_NUMBER(a)+AS_NUMBER(b)));
                } else {
                    runtimeError(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: if (!binaryOp(vm, VAL_NUMBER, substractOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_MULTIPLY: if (!binaryOp(vm, VAL_NUMBER, multiplyOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_DIVIDE: if (!binaryOp(vm, VAL_NUMBER, divideOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_NOT: push(&vm->stack, BOOL_VAL(isFalsy(pop(&vm->stack)))); break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(&vm->stack, 0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(&vm->stack, NUMBER_VAL(-AS_NUMBER(pop(&vm->stack))));
                break;
            }
            case OP_PRINT:
                printValue(pop(&vm->stack));
                printf("\n");
                break;
            case OP_POP:
                pop(&vm->stack);
                break;
            case OP_DEFINE_GLOBAL: {
                ObjString* global = AS_STRING(READ_CONSTANT(vm));
                tableSet(&vm->globals, global, pop(&vm->stack));
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(VM* vm, const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(vm, source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult res = run(vm);
    freeChunk(&chunk);

    return res;
}


