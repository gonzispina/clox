//
// Created by gonzalo on 13/12/21.
//

#include <stdlib.h>
#include <string.h>
#include <iso646.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

static uint32_t FNV1FHash(const char* text, int length) {
    uint32_t hash = FNV1_OFFSET_BASIS;
    for (int i = 0; i < length; i++) {
        hash = hash xor (uint8_t)text[i];
        hash = hash * FNV1_PRIME;
    }
    return hash;
}

uint32_t hashString(char* text, int length) {
    return FNV1FHash(text, length);
}

void initTable(Table* t) {
    t->capacity = 0;
    t->count = 0;
    t->entries = NULL;
}

void freeTable(Table *t) {
    FREE_ARRAY(Entry, t->entries, t->capacity);
    initTable(t);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    for(;;) {
        Entry* e = &entries[index];
        if (e->key == key) {
            return e;
        }

        // e->key == NULL
        if (!IS_NIL(e->value)) {
            // If we've already found a tombstone, we must return it.
            return tombstone != NULL ? tombstone : e;
        }

        if (tombstone == NULL) tombstone = e;

        index = (index + 1) % capacity;
    }
}

static bool needsResize(Table* t) {
    return (float)(t->count + 1) / (float)t->capacity > TABLE_MAX_LOAD;
}

static void adjustCapacity(Table* t) {
    int capacity = GROW_CAPACITY(t->capacity);
    Entry* entries = (Entry*)malloc(sizeof(Entry) * capacity);

    t->count = 0;
    for (int i = 0; i < t->capacity; ++i) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;

        if (t->entries == NULL || t->entries[i].key == NULL) {
            continue;
        }

        Entry e = t->entries[i];
        Entry* dest = findEntry(entries, capacity, e.key);
        dest->key = e.key;
        dest->value = e.value;
        t->count++;
    }

    FREE_ARRAY(Entry, t->entries, t->capacity);
    t->capacity = capacity;
    t->entries = entries;
}

bool tableSet(Table *t, ObjString *key, Value value) {
    if (needsResize(t)) adjustCapacity(t);

    Entry* entry = findEntry(t->entries, t->capacity, key);
    bool isNew = entry->key == NULL;
    if (isNew && !IS_NIL(entry->value)) t->count++;

    entry->key = key;
    entry->value = value;
    return isNew;
}

bool tableGet(Table *t, ObjString *key, Value *value) {
    if (t->count == 0) return false;

    Entry* e = findEntry(t->entries, t->capacity, key);
    if (e->key == NULL) return false;

    *value = e->value;
    return true;
}

bool tableDelete(Table *t, ObjString *key) {
    if (t->count == 0) return false;

    Entry* e = findEntry(t->entries, t->capacity, key);
    if (e->key == NULL) return false;

    e->key == NULL;
    e->value = NIL_VAL;
    return true;
}

void tableCopy(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* t, const char* chars, int length, uint32_t hash) {
    if (t->count == 0) return NULL;

    uint32_t index = (hash % t->capacity);
    for (;;) {
        Entry* entry = &t->entries[index];
        if (entry->key == NULL) {
            if (!IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1 % t->capacity);
    }
}


