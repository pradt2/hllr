#include "gc.hpp"

struct Node {
    Node *nextNode;
    long value;
};

Type NodeType {
    .requiredWords = sizeof(Node),
    .pointersCount = 1,
};

void recursiveMethod(Thread* thread, size_t pointerStackOffset, unsigned int recursionLimit = 100) {
    const size_t pointerSpaceDemand = 1;

    thread->pointerStack[pointerStackOffset + 0] = (uintptr_t) alloc(thread, &NodeType);
    ((Node*) thread->pointerStack[pointerStackOffset + 0])->value = recursionLimit;

    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode = (Node*) alloc(thread, &NodeType);
    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value = recursionLimit;

    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode = (Node*) alloc(thread, &NodeType);
    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value = recursionLimit;


    if (recursionLimit > 0) {
        recursiveMethod(thread, pointerStackOffset + pointerSpaceDemand, recursionLimit - 1);
    }

    if (((Node*) thread->pointerStack[pointerStackOffset + 0])->value != recursionLimit
        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value != recursionLimit
        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value != recursionLimit) {
        printf("%ld %ld %ld\n",((Node*) thread->pointerStack[pointerStackOffset + 0])->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value);
        exit(13);
    }

    thread->pointerStack[pointerStackOffset + 0] = 0;
}

int main() {


    Thread* main = initRuntime();

    for (int i = 0; i < 1<<20; i++) {
        recursiveMethod(main, 0, 100);
    }

    main->isActive = false;

    sleep(1);

    gc();
    gc();

    if (main->heapPage->nextPage) {
        printf("Too many pages!\n");
        exit(17);
    }

    shutdownRuntime();
    return 0;
}
