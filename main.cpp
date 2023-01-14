#include <iostream>
#include "gc.hpp"

struct Node {
//    Node *nextNode;
//    long value;
};

Type NodeType {
    .requiredWords = sizeof(Node),
    .pointersCount = 0,
};

Node* recursiveMethod(ThreadRuntime* thread, unsigned int recursionLimit = 100) {
    auto raii = thread->allocator.getRAII();
    Node *node, *node1;

    node = (Node*) thread->allocator.alloc(&NodeType);
//    node->value = recursionLimit;

//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode = (Node*) alloc(thread, &NodeType);
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value = recursionLimit;
//
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode = (Node*) alloc(thread, &NodeType);
//    ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value = recursionLimit;

    if (recursionLimit > 0) {
        node1 = recursiveMethod(thread, recursionLimit - 1);
    } else return node;

//    if (((Node*) thread->pointerStack[pointerStackOffset + 0])->value != recursionLimit
//        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value != recursionLimit
//        || ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value != recursionLimit) {
//        printf("%ld %ld %ld\n",((Node*) thread->pointerStack[pointerStackOffset + 0])->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->value, ((Node*) thread->pointerStack[pointerStackOffset + 0])->nextNode->nextNode->value);
//        exit(13);
//    }

    if (node < node1) {
        return node;
    } else return node1;
}

int mainYY() {
    const auto size = 1024;
    Node *nodes[size] = {nullptr};

    auto start = std::chrono::steady_clock::now();

    long iters = 1024 * 1024 * 2;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < size; i++) {
            delete nodes[i];
            nodes[i] = new Node();
        }
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((size * iters) / ms * 1000) / 1000000.0 << std::endl;

    return 0;
}

int realMain(ThreadRuntime *runtime) {
    const auto size = 1024;
    Node *nodes[size] = {nullptr};

    auto start = std::chrono::steady_clock::now();

    long iters = 1024 * 1024 * 3;

    for (int iter = 0; iter < iters; iter++) {
        auto raii = runtime->allocator.getRAII();
        for (int i = 0; i < size; i++) {
            runtime->allocator.dealloc(nodes[i]);
            nodes[i] = (Node*) runtime->allocator.alloc(&NodeType);
        }
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((size * iters) / ms * 1000) / 1000000.0 << std::endl;

    runtime->isActive = false;

    return 0;
}

int mainZ() {
    ThreadRuntime* main = initRuntime();
    return realMain(main);
}

int main() {
    ThreadRuntime* main = initRuntime();

    const auto iters = 1 << 24;
    const auto recursion = 100;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++) {
        recursiveMethod(main, recursion);
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((recursion * iters) / ms * 1000) / 1000000.0 << std::endl;

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
