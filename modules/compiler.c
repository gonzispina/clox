//
// Created by gonzalo on 1/12/21.
//

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "scanner.h"

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

    fprintf(stderr, "[line %d] Error", token->line);

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

static void grouping(Parser* p) {
    expression(p);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* p) {
    double value = strtod(p->previous.start, NULL);
    emitConstant(p, NUMBER_VAL(value));
}

static void unary(Parser* p) {
    TokenType operatorType = p->previous.type;

    parsePrecedence(p, PREC_UNARY);

    if (operatorType == TOKEN_MINUS) {
        emitByte(p, OP_NEGATE);
    }
}

static void binary(Parser* p) {
    TokenType operatorType = p->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(p, (Precedence)rule->precedence + 1);

    switch (operatorType) {
        case TOKEN_PLUS: emitByte(p, OP_ADD); break;
        case TOKEN_MINUS: emitByte(p, OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(p, OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(p, OP_DIVIDE); break;
        default: return; // Unreachable
    }
}

static void literal(Parser* p) {
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
        [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
        [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
        [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
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

static void parsePrecedence(Parser* p, Precedence precedence) {
    advance(p);
    ParseFn prefixRule = getRule(p->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(p, "Expect expression.");
        return;
    }

    prefixRule(p);

    while(precedence <= getRule(p->current.type)->precedence) {
        advance(p);
        ParseFn infixRule = getRule(p->previous.type)->infix;
        infixRule(p);
    }
}

static void expression(Parser* p) {
    parsePrecedence(p, PREC_ASSIGNMENT);
}

bool compile(const char* source, Chunk* chunk) {
    s = initScanner(source);
    Parser* p = initParser();
    compilingChunk = chunk;

    advance(p);
    expression(p);
    consume(p, TOKEN_EOF, "Expect end of expression.");
    endCompiler(p);

    bool compiled = !p->hadError;

    free(p);
    free(s);

    return compiled;
}



