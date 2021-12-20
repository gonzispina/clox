//
// Created by gonzalo on 1/12/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "scanner.h"
#include "vm.h"
#include "strings.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Chunk* compilingChunk;
Scanner* s;

Parser *initParser() {
    Parser* p = (Parser*)malloc(sizeof (Parser));
    if (p == NULL) {
        exit(1);
    }
    p->hadError = false;
    p->panicMode = false;
    return p;
}

static void errorAt(Parser* p, Token* token, const char* message) {
    if (p->panicMode) return;
    p->panicMode = true;

    fprintf(stderr, "\n[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    p->hadError = true;
}

static void error(Parser* p, const char* message) {
    errorAt(p, &p->previous, message);
}

static void errorAtCurrent(Parser* p) {
    errorAt(p, &p->current, p->current.start);
}

static void advance(Parser* p) {
    p->previous = p->current;

    for (;;) {
        p->current = scanToken(s);
        if (p->current.type != TOKEN_ERROR) break;

        errorAtCurrent(p);
    }
}

static void consume(Parser* p, TokenType type, const char* message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }

    errorAtCurrent(p);
}

static bool match(Parser* p, TokenType type) {
    if (p->current.type == type) {
        advance(p);
        return true;
    }

    return false;
}

static void synchronize(Parser* p) {
    p->panicMode = false;

    while (p->current.type != TOKEN_EOF) {
        if (p->previous.type == TOKEN_SEMICOLON) return;
        switch (p->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:; // Do nothing.
        }

        advance(p);
    }
}

static Chunk* currentChunk() {
    return compilingChunk;
}

static void emitByte(Parser* p, uint8_t byte) {
    writeChunk(currentChunk(), byte, p->previous.line);
}

static void emitBytes(Parser* p, uint8_t byte1, uint8_t byte2) {
    emitByte(p, byte1);
    emitByte(p, byte2);
}

static uint8_t makeConstant(Parser* p, Value v) {
    int constant = addConstant(currentChunk(), v);
    if (constant > UINT8_MAX) {
        error(p, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Parser* p, Value v) {
    emitBytes(p, OP_CONSTANT, makeConstant(p, v));
}

static void endCompiler(Parser* p) {
    emitByte(p, OP_RETURN);
#ifdef DEBUG_PRINT_CODE
    if (!p->hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void grouping(VM* vm, Parser* p) {
    expression(vm, p);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VM* _, Parser* p) {
    double value = strtod(p->previous.start, NULL);
    emitConstant(p, NUMBER_VAL(value));
}

static void string(VM* vm, Parser* p) {
    emitConstant(p, OBJ_VAL(copyString(vm, p->previous.start + 1, p->previous.length - 2)));
}

static void unary(VM* vm, Parser* p) {
    TokenType operatorType = p->previous.type;

    parsePrecedence(vm, p, PREC_UNARY);

    if (operatorType == TOKEN_MINUS)     emitByte(p, OP_NEGATE);
    else if (operatorType == TOKEN_BANG) emitByte(p, OP_NOT);
}

static void binary(VM* vm, Parser* p) {
    TokenType operatorType = p->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(vm, p, (Precedence)rule->precedence + 1);

    switch (operatorType) {
        case TOKEN_PLUS: emitByte(p, OP_ADD); break;
        case TOKEN_MINUS: emitByte(p, OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(p, OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(p, OP_DIVIDE); break;
        case TOKEN_EQUAL_EQUAL: emitByte(p, OP_EQUAL); break;
        case TOKEN_BANG_EQUAL: emitByte(p,OP_EQUAL); emitByte(p,OP_NOT); break;
        case TOKEN_GREATER: emitByte(p, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitByte(p,OP_LESSER); emitByte(p,OP_NOT); break;
        case TOKEN_LESS: emitByte(p, OP_LESSER); break;
        case TOKEN_LESS_EQUAL: emitByte(p,OP_GREATER); emitByte(p,OP_NOT); break;
        default: return; // Unreachable
    }
}

static void literal(VM* _, Parser* p) {
    switch (p->previous.type) {
        case TOKEN_TRUE: emitByte(p, OP_TRUE); break;
        case TOKEN_FALSE: emitByte(p, OP_FALSE); break;
        case TOKEN_NIL: emitByte(p, OP_NIL); break;
        default: return; // Unreachable
    };
}

ParseRule rules[] = {
        [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
        [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
        [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
        [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
        [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
        [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
        [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
        [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
        [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
        [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
        [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
        [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
        [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
        [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void parsePrecedence(VM* vm, Parser* p, Precedence precedence) {
    advance(p);
    ParseFn prefixRule = getRule(p->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(p, "Expect expression.");
        return;
    }

    prefixRule(vm, p);

    while(precedence <= getRule(p->current.type)->precedence) {
        advance(p);
        ParseFn infixRule = getRule(p->previous.type)->infix;
        infixRule(vm, p);
    }
}

static void expression(VM* vm, Parser* p) {
    parsePrecedence(vm, p, PREC_ASSIGNMENT);
}

static void expressionStatement(VM* vm, Parser* p) {
    expression(vm, p);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(p, OP_POP);
}

static void printStatement(VM* vm, Parser* p) {
    expression(vm, p);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(p, OP_PRINT);
}

static void statement(VM* vm, Parser* p) {
    if (match(p, TOKEN_PRINT)) {
        printStatement(vm, p);
        return;
    }

    expressionStatement(vm, p);
}

static uint8_t identifierConstant(VM* vm, Parser* p) {
    return makeConstant(p, OBJ_VAL(copyString(vm, p->previous.start, p->previous.length)));
}

static uint8_t parseVariable(VM* vm, Parser* p, const char* errorMessage) {
    consume(p, TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(vm, p);
}

static void varDeclaration(VM* vm, Parser* p) {
    uint8_t global = parseVariable(vm, p, "Expect variable name");

    if (match(p, TOKEN_EQUAL)) {
        expression(vm, p);
    } else {
        emitByte(p, OP_NIL);
    }

    consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    emitBytes(p, OP_DEFINE_GLOBAL, global);
}

static void declaration(VM* vm, Parser* p) {
    if (match(p, TOKEN_VAR)) {
        varDeclaration(vm, p);
    } else {
        statement(vm, p);
    }

    if (p->panicMode) synchronize(p);
}

bool compile(VM* vm, const char* source, Chunk* chunk) {
    s = initScanner(source);
    Parser* p = initParser();
    compilingChunk = chunk;

    advance(p);
    while (!match(p, TOKEN_EOF)) {
        declaration(vm, p);
    }

    consume(p, TOKEN_EOF, "Expect end of expression.");
    endCompiler(p);

    bool compiled = !p->hadError;

    free(p);
    free(s);

    return compiled;
}



