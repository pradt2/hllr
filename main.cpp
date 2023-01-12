#include <iostream>
#include "gc.hpp"

struct Node {
//    Node *nextNode;
    long value;
};

Type NodeType {
    .requiredWords = sizeof(Node),
    .pointersCount = 0,
};

Node* recursiveMethod(Thread* thread, size_t pointerStackOffset, unsigned int recursionLimit = 100) {
    const size_t pointerSpaceDemand = 2;

    thread->pointerStack[pointerStackOffset + 0] = (uintptr_t) alloc(thread, &NodeType);
    ((Node*) thread->pointerStack[pointerStackOffset + 0])->value = recursionLimit;

//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode = (Node*) alloc(thread, &NodeType);
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value = recursionLimit;
//
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode = (Node*) alloc(thread, &NodeType);
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value = recursionLimit;

    if (recursionLimit > 0) {
        thread->pointerStack[pointerStackOffset + 1] = (uintptr_t) recursiveMethod(thread, pointerStackOffset + pointerSpaceDemand, recursionLimit - 1);
    } else return (Node*) thread->pointerStack[pointerStackOffset + 0];

//    if (((Node*) thread->pointerStack[pointerStackOffset + 0])->value != recursionLimit
//        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value != recursionLimit
//        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value != recursionLimit) {
//        printf("%ld %ld %ld\n",((Node*) thread->pointerStack[pointerStackOffset + 0])->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value);
//        exit(13);
//    }

    if (thread->pointerStack[pointerStackOffset + 0] < thread->pointerStack[pointerStackOffset + 1]) {
        return (Node*) thread->pointerStack[pointerStackOffset + 0];
    } else return (Node*) thread->pointerStack[pointerStackOffset + 1];
}

int main() {
    Thread* main = initRuntime();

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1<<24; i++) {
        recursiveMethod(main, 0, 100);
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((100L * (1 << 22)) / ms * 1000) / 1000000.0 << std::endl;

    main->isActive = false;

//    sleep(1);
//
//    gc();
//    gc();
//
//    if (main->heapPage->nextPage) {
//        printf("Too many pages!\n");
//        exit(17);
//    }

//    shutdownRuntime();
    return 0;
}

struct Ex1 : public std::exception {};

struct Ex2 : public Ex1 {};

struct Ex3 : public Ex1 {};

void throw1() {
    throw Ex3();
}

void throw2() {
    throw Ex2();
}

int mainX() {
    try {
        throw 1;
    } catch (int &e) {
        printf("Caught super exception %d!\n", e);
    } catch (Ex3 &e) {
        printf("Caught super exception 3!\n");
    }
    return 0;
}
