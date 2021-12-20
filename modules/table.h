//
// Created by gonzalo on 13/12/21.
//

#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include <stdio.h>
#include "common.h"
#include "value.h"

#define FNV1_OFFSET_BASIS 2166136261
#define FNV1_PRIME 16777619
#define TABLE_MAX_LOAD 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

uint32_t hashString(char* text, int length);

void initTable(Table* t);
void freeTable(Table* t);
bool tableSet(Table* t, ObjString* key, Value value);
bool tableGet(Table* t, ObjString* key, Value* value);
bool tableDelete(Table* t, ObjString* key);
void tableCopy(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif //CLOX_TABLE_H
