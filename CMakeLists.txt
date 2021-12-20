cmake_minimum_required(VERSION 3.20)
project(clox C)

set(CMAKE_C_STANDARD 99)

add_executable(clox main.c modules/chunk.c modules/memory.h modules/memory.c modules/debug.h modules/debug.c modules/value.h modules/value.c modules/vm.h modules/vm.c modules/compiler.h modules/compiler.c modules/scanner.c modules/scanner.h modules/object.c modules/object.h modules/table.c modules/table.h modules/strings.c modules/strings.h)
