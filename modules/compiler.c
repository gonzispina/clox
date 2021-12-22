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

Compiler* c = NULL;
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

static void initCompiler(Compiler* compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    c = compiler;
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

static bool check(Parser* p, TokenType type) {
    return p->current.type == type;
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

static uint8_t makeIdentifier(Parser* p, Value v) {
    int constant = addIdentifier(currentChunk(), v);
    if (constant > UINT8_MAX) {
        error(p, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}


static void emitConstant(Parser* p, Value v) {
    emitBytes(p, OP_CONSTANT, makeConstant(p, v));
}

static int emitJump(Parser* p, uint8_t instruction) {
    emitByte(p, instruction);
    // We are going to need 16bit instruction to handle jumps of 2¹⁶ bytes of code
    emitByte(p, 0xff); emitByte(p, 0xff); // 255 because it is easy to use with bitwise operations
    return currentChunk()->count - 2;
}

static void patchJump(Parser* p, int offset) {
    int jump = currentChunk()->count - 2 - offset;
    if (jump > UINT16_MAX) {
        error(p, "Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void beginScope() {
    c->scopeDepth++;
}

static void endScope(Parser* p) {
    c->scopeDepth--;

    // We emit a POP for every variable in the local scope
    while (c->localCount > 0 && c->locals[c->localCount - 1].depth > c->scopeDepth) {
        emitByte(p, OP_POP);
        c->localCount--;
    }
}

static void endCompiler(Parser* p) {
    emitByte(p, OP_RETURN);
#ifdef DEBUG_PRINT_CODE
    if (!p->hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void grouping(VM* vm, Parser* p, bool _) {
    expression(vm, p);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VM* vm, Parser* p, bool _) {
    double value = strtod(p->previous.start, NULL);
    emitConstant(p, NUMBER_VAL(value));
}

static void string(VM* vm, Parser* p, bool _) {
    emitConstant(p, OBJ_VAL(copyString(vm, p->previous.start + 1, p->previous.length - 2)));
}

static uint8_t identifierConstant(VM* vm, Parser* p) {
    return makeIdentifier(p, OBJ_VAL(copyString(vm, p->previous.start, p->previous.length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Parser* p, Token* name) {
    for  (int i = compiler->localCount - 1; i >= 0; i--) {
        if (identifiersEqual(&compiler->locals[i].name, name)) {
            if (c->locals[i].depth == -1) {
                error(p, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void namedVariable(VM* vm, Parser* p, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(c, p, &p->previous);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(vm, p);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(p, TOKEN_EQUAL)) {
        expression(vm, p);
        emitBytes(p, setOp, (uint8_t)arg);
    } else {
        emitBytes(p, getOp, (uint8_t)arg);
    }
}

static void addLocal(Parser* p, Token name) {
    if (c->localCount == UINT8_COUNT) {
        error(p, "Too many local variables in function.");
        return;
    }

    for (int i = c->localCount - 1; i >= 0; i--) {
        Local* local = &c->locals[i];
        if (local->depth != -1 && local->depth < c->scopeDepth) {
            break;
        }

        if (identifiersEqual(&name, &local->name)) {
            error(p, "Already a variable with this name in this scope.");
        }
    }

    Local* local = &c->locals[c->localCount++];
    local->name = name;
    local->depth = -1;
}

static void declareVariable(Parser* p) {
    if (c->scopeDepth == 0) return;
    addLocal(p, p->previous);
}

static void markInitialized() {
    c->locals[c->localCount - 1].depth = c->scopeDepth;
}

static void defineVariable(VM* vm, Parser* p, uint8_t var) {
    if (c->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitBytes(p, OP_DEFINE_GLOBAL, var);
}

static uint8_t parseVariable(VM* vm, Parser* p, const char* errorMessage) {
    consume(p, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(p);
    if (c->scopeDepth > 0) return 0;

    return identifierConstant(vm, p);
}


static void variable(VM* vm, Parser* p, bool canAssign) {
    namedVariable(vm, p, canAssign);
}

static void unary(VM* vm, Parser* p, bool _) {
    TokenType operatorType = p->previous.type;

    parsePrecedence(vm, p, PREC_UNARY);

    if (operatorType == TOKEN_MINUS)     emitByte(p, OP_NEGATE);
    else if (operatorType == TOKEN_BANG) emitByte(p, OP_NOT);
}

static void binary(VM* vm, Parser* p, bool _) {
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

static void literal(VM* vm, Parser* p, bool _) {
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
        [TOKEN_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(vm, p, canAssign);

    while(precedence <= getRule(p->current.type)->precedence) {
        advance(p);
        ParseFn infixRule = getRule(p->previous.type)->infix;
        infixRule(vm, p, canAssign);
    }

    if (canAssign && match(p, TOKEN_EQUAL)) {
        error(p, "Invalid assignment target.");
    }
}

static void expression(VM* vm, Parser* p) {
    parsePrecedence(vm, p, PREC_ASSIGNMENT);
}

static void blockStatement(VM* vm, Parser* p) {
    beginScope();
    while (!check(p, TOKEN_RIGHT_BRACE) && !check(p, TOKEN_EOF)) {
        declaration(vm, p);
    }

    consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    endScope(p);
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

static void ifStatement(VM* vm, Parser* p) {
    consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(vm, p);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    beginScope();
    int thenJump = emitJump(p, OP_JUMP_IF_FALSE);
    emitByte(p, OP_POP);
    statement(vm, p);

    int elseJump = emitJump(p, OP_JUMP);
    patchJump(p, thenJump);
    emitByte(p, OP_POP);

    if (match(p, TOKEN_ELSE)) statement(vm, p);
    patchJump(p, elseJump);

    endScope(p);
}

void statement(VM* vm, Parser* p) {
    if (match(p, TOKEN_PRINT)) {
        printStatement(vm, p);
        return;
    } else if (match(p, TOKEN_IF)) {
        ifStatement(vm, p);
        return;
    } else if (match(p, TOKEN_LEFT_BRACE)) {
        blockStatement(vm, p);
        return;
    }

    expressionStatement(vm, p);
}

static void varDeclaration(VM* vm, Parser* p) {
    uint8_t name = parseVariable(vm, p, "Expect variable name");

    if (match(p, TOKEN_EQUAL)) {
        expression(vm, p);
    } else {
        emitByte(p, OP_NIL);
    }

    consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(vm, p, name);
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
    Compiler compiler;
    initCompiler(&compiler);
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



