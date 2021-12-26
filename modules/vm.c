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
    stack->top = &stack->values[0];
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
    return *(stack->top - distance - 1);
}

static void freeStack(Stack stack) {

}

static bool isFalsey(Value v) {
    return IS_NIL(v) || (IS_BOOL(v) && !AS_BOOL(v));
}


static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    size_t instruction = frame->ip - frame->function->chunk.code - 1;
    int line = frame->function->chunk.lines[instruction];
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
    vm->frameCount = 0;
    return vm;
}

void freeVM(VM* vm) {
    freeObjects(vm->objects);
    freeTable(&vm->strings);
    freeTable(&vm->globals);
    free(vm);
}

static uint8_t READ_BYTE(CallFrame* frame) {
    return *frame->ip++;
}

static uint16_t READ_16_BYTE(CallFrame* frame) {
    uint8_t first = READ_BYTE(frame);
    uint8_t second = READ_BYTE(frame);
    return (uint16_t)((first << 8) | second);
}

static Value READ_CONSTANT(CallFrame* frame) {
    return frame->function->chunk.constants.values[READ_BYTE(frame)];
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

static bool validateNumbers(VM* vm, CallFrame* frame, Value a, Value b) {
    if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
        runtimeError(vm, "Operands must be a numbers.");
        return false;
    }
    return true;
}

static bool validateEqualType(VM* vm, CallFrame* frame, Value a, Value b) {
    if (a.type != b.type) {
        runtimeError(vm, "Operands must be of the same type.");
        return false;
    }
    return true;
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
static bool binaryOp(VM* vm, CallFrame* frame, ValueType type, BinaryFn op) {
    Value b = pop(&vm->stack);
    Value a = pop(&vm->stack);
    if (type == VAL_NUMBER && !validateNumbers(vm, frame, a, b)) return false;
    if (type == VAL_BOOL && !validateEqualType(vm, frame, a, b)) return false;
    op(vm, a, b);
    return true;
}

// {var a = "a"; var b="b"; print(a + " " + b);}
static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printStack(&vm->stack);
    // disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE(frame)) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT(frame);
                push(&vm->stack, constant);
                break;
            }
            case OP_NIL: push(&vm->stack, NIL_VAL); break;
            case OP_TRUE: push(&vm->stack, BOOL_VAL(true)); break;
            case OP_FALSE: push(&vm->stack, BOOL_VAL(false)); break;
            case OP_EQUAL: if (!binaryOp(vm, frame, VAL_BOOL, equalOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_GREATER: if (!binaryOp(vm, frame, VAL_BOOL, greaterOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_LESSER: if (!binaryOp(vm, frame, VAL_BOOL, lesserOp)) return INTERPRET_RUNTIME_ERROR; break;
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
            case OP_SUBTRACT: if (!binaryOp(vm, frame, VAL_NUMBER, substractOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_MULTIPLY: if (!binaryOp(vm, frame, VAL_NUMBER, multiplyOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_DIVIDE: if (!binaryOp(vm, frame, VAL_NUMBER, divideOp)) return INTERPRET_RUNTIME_ERROR; break;
            case OP_NOT: push(&vm->stack, BOOL_VAL(isFalsey(pop(&vm->stack)))); break;
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
                ObjString* global = AS_STRING(READ_CONSTANT(frame));
                tableSet(&vm->globals, global, pop(&vm->stack));
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONSTANT(frame));
                Value value;
                if (!tableGet(&vm->globals, name, &value)) {
                    runtimeError(vm,"Undefined variable '%s.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(&vm->stack, value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONSTANT(frame));
                if (tableSet(&vm->globals, name, peek(&vm->stack, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm,"Undefined variable '%s.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE(frame);
                push(&vm->stack, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE(frame);
                frame->slots[slot] = peek(&vm->stack, 0);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_16_BYTE(frame);
                frame->ip += (uint16_t)isFalsey(peek(&vm->stack, 0)) * offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_16_BYTE(frame);
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_16_BYTE(frame);
                frame->ip -= offset;
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(&vm->stack, OBJ_VAL(function));
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm->stack.values;

    InterpretResult res = run(vm);

    return res;
}


