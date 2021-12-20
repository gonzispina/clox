//
// Created by gonzalo on 1/12/21.
//

#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "vm.h"
#include "scanner.h"

typedef struct Parser {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser* initParser();

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(VM*, Parser*, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

bool compile(VM*, const char* source, Chunk* chunk);

static void parsePrecedence(VM*, Parser*, Precedence);
static void expression(VM*, Parser*);
static ParseRule* getRule(TokenType);

#endif //CLOX_COMPILER_H
