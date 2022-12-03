#include "gc.hpp"

struct Node {
    Node *nextNode;
    long value;
};

Type NodeType {
    .requiredWords = sizeof(Node),
    .pointersCount = 1,
};

void recursiveMethod(Thread* thread, PointerPage **nextPageField, unsigned int recursionLimit = 100) {
    struct {
        PointerPage page = {
                .nextPage = nullptr,
                .pointerCount = 1,
        };
        Node *ptr1 = nullptr;
    } pointers;

    *nextPageField = &pointers.page;

    pointers.ptr1 = (Node*) alloc(thread, &NodeType);
    pointers.ptr1->value = recursionLimit;

    if (recursionLimit > 0) {
        recursiveMethod(thread, &pointers.page.nextPage, recursionLimit - 1);
    }

    if (pointers.ptr1->value != recursionLimit) {
        exit(13);
    }

    *nextPageField = nullptr;
}

int main() {
    struct {
        PointerPage page = {
                .nextPage = nullptr,
                .pointerCount = 0,
        };
    } pointers;

    Thread* main = initRuntime(&pointers.page);

    for (int i = 0; i < 1<<13; i++) {
        recursiveMethod(main, &pointers.page.nextPage, 1000);
//        gc();
    }

    shutdownRuntime();
    return 0;
}
