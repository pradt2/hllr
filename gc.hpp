#ifndef GC_GC_HPP
#define GC_GC_HPP

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <thread>

const size_t HEAP_PAGE_SIZE_WORDS = 1280000;

enum struct Colour: char {
    Green = 0, Blue = 1,
};

struct Type;                                // characterises every object type
struct Runtime;
struct GC;
struct Thread;
struct HeapPage;
struct HeapAlloc;
struct PointerPage;

struct Type {
    size_t requiredWords;                   // how many words are needed to allocate
    size_t pointersCount;                   // how many pointers an object of this Type stores
};

struct Runtime {
    GC *gc;                                 // garbage collector structure
    std::mutex threadMutex;                 // mutex to coordinate adding/removing threads
    Thread *mainThread;                     // pointer to the main thread
};

struct GC {
    std::mutex gcMutex;                     // mutex to coordinate garbage collection
    std::thread *gcThread;                  // thread that coordinates the garbage collection
    volatile Colour colour;                 // colour to stain new allocations with
};

struct Thread {
    HeapPage *heapPage;                     // pointer to the first heap page
    HeapPage *lastPage;                     // pointer to the last heap page
    PointerPage *pointerPage;               // pointer to the first pointer page
    Thread *nextThread;                     // pointer to the next thread
    bool isActive;                          // whether the thread is actually used (i.e. has a backing std::thread)
};

struct HeapPage {
    HeapPage *nextPage;                     // pointer to the next heap page
    size_t usableWords;                     // size where allocations can be placed, i.e. excluding this header, as number of words
    HeapAlloc* freeAllocHint;               // may point to a free allocation on the heap (nullptr otherwise)
    bool isSinglePurpose;                   // whether it has been allocated for one big object
};

const size_t HEAP_PAGE_HEADER_BYTES = sizeof(HeapPage);
const size_t HEAP_PAGE_HEADER_WORDS = HEAP_PAGE_HEADER_BYTES / sizeof(uintptr_t);

struct HeapAlloc {
    Type *type;                             // type of object currently held, NULL means the allocation is free
    size_t usableWords;                     // size where object data can be placed, as number of words
    Colour colour;                          // stain colour of the allocation, used by the GC
};

const size_t HEAP_ALLOC_HEADER_BYTES = sizeof(HeapAlloc);
const size_t HEAP_ALLOC_HEADER_WORDS = HEAP_ALLOC_HEADER_BYTES / sizeof(uintptr_t);

struct PointerPage {
    PointerPage *nextPage;                  // pointer to the next page (lower down the stack)
    size_t pointerCount;                    // how many pointers does this page hold
};

const size_t POINTER_PAGE_HEADER_BYTES = sizeof(PointerPage);
const size_t POINTER_PAGE_HEADER_WORDS = POINTER_PAGE_HEADER_BYTES / sizeof(uintptr_t);

extern Runtime *RUNTIME;

Thread* initRuntime(PointerPage *pointerPage);
void shutdownRuntime();

void *alloc(Thread *thread, Type *type);
void gc();
void addThread();
void removeThread();

void printHeap(Thread *thread);
void printHeapSummary(Thread *thread);

#endif //GC_GC_HPP
