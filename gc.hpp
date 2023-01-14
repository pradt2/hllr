#ifndef GC_GC_HPP
#define GC_GC_HPP

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <thread>

const size_t HEAP_PAGE_SIZE_WORDS = 128000;

enum struct Colour: char {
    Green = 0, Blue = 1,
};

struct Type;                                // characterises every object type
struct Runtime;
struct GC;
struct Allocator;
struct ThreadRuntime;
struct HeapPage;
struct HeapAlloc;

struct Type {
    size_t requiredWords;                   // how many words are needed to allocate
    size_t pointersCount;                   // how many pointers an object of this Type stores
};

struct Runtime {
    GC *gc;                                 // garbage collector structure
    std::mutex threadMutex;                 // mutex to coordinate adding/removing threads
    ThreadRuntime *mainThread;                     // pointer to the main thread
};

struct GC {
    std::mutex gcMutex;                     // mutex to coordinate garbage collection
    std::thread *gcThread;                  // thread that coordinates the garbage collection
    volatile Colour colour;                 // colour to stain new allocations with
};

struct Allocator {
    HeapPage *firstPage;                     // pointer to the first heap page
    HeapPage *lastPage;                     // pointer to the last heap page
    uintptr_t pointerIdx = 0;               // keeps track of the current position in the pointer stack
    uintptr_t pointerStack[4096] = {0};     // keeps track of stack GC roots

public:
    explicit Allocator();

    class ReserveStackSpaceRAII {
    private:
        Allocator *allocator;
        uintptr_t oldIdx;

    public:
        ReserveStackSpaceRAII(Allocator *alloc) {
            this->allocator = alloc;
            this->oldIdx = alloc->pointerIdx;
        }

        ~ReserveStackSpaceRAII() {
            this->allocator->pointerIdx = this->oldIdx;
        }
    };

    inline ReserveStackSpaceRAII getRAII() {
        return {this};
    }

    void *alloc(Type *type);

    void dealloc(void *ptr);
};

struct ThreadRuntime {
    ThreadRuntime *nextThread;              // pointer to the next thread
    Allocator allocator;                    // handles on-heap allocations
    bool isActive;                          // whether the thread is actually used (i.e. has a backing std::thread)
};

struct HeapPage {
    HeapPage *nextPage;                     // pointer to the next heap page
    size_t usableWords;                     // size where allocations can be placed, i.e. excluding this header, as number of words
    HeapAlloc* middleFreeAlloc;             // may point to a free allocation somewhere in the middle of the page
    HeapAlloc* lastFreeAlloc;               // may point to a free allocation on the heap (nullptr otherwise)
    bool isSinglePurpose;                   // whether it has been allocated for one big object
    Colour colour;                          // heap page colour for GC purposes
};

const size_t HEAP_PAGE_HEADER_BYTES = sizeof(HeapPage);
const size_t HEAP_PAGE_HEADER_WORDS = HEAP_PAGE_HEADER_BYTES / sizeof(uintptr_t);

struct HeapAlloc {
    Type *type;                             // type of object currently held, NULL means the allocation is free
    size_t usableWords;                     // size where object data can be placed, as number of words
    HeapPage *parentPage;                   // the heap page where this allocation resides
    Colour colour;                          // stain colour of the allocation, used by the GC
};

const size_t HEAP_ALLOC_HEADER_BYTES = sizeof(HeapAlloc);
const size_t HEAP_ALLOC_HEADER_WORDS = HEAP_ALLOC_HEADER_BYTES / sizeof(uintptr_t);

extern Runtime *RUNTIME;

ThreadRuntime* initRuntime();
void shutdownRuntime();

void gcST();
void gc();
void addThread();
void removeThread();

void printHeap(ThreadRuntime *thread);
void printHeapSummary(ThreadRuntime *thread);

#endif //GC_GC_HPP
