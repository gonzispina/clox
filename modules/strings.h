//
// Created by gonzalo on 20/12/21.
//

#ifndef CLOX_STRINGS_H
#define CLOX_STRINGS_H

#include "object.h"

ObjString* copyString(VM* vm, char* chars, int length);
ObjString* takeString(VM* vm, char* chars, int length);

#endif //CLOX_STRINGS_H
