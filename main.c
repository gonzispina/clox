#include "modules/common.h"
#include "modules/chunk.h"
#include "modules/debug.h"
#include "modules/vm.h"

int main(int argc, const char* argv[]) {
    VM vm = initVM();

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN, 123);

    int index = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, index, 123);

    disassembleChunk(&chunk, "test chunk");
    interpret(&vm, &chunk);

    freeVM(&vm);
    freeChunk(&chunk);
    return 0;
}
