#include <cstring>
#include <iostream>
#include <queue>
#include "gc.hpp"

Runtime* RUNTIME;

HeapAlloc *getNextAlloc(HeapPage *page, HeapAlloc *alloc = nullptr) {
    if (!alloc) return (HeapAlloc*) ((uintptr_t) page + HEAP_PAGE_HEADER_BYTES);
    auto nextAlloc = (HeapAlloc*) ((uintptr_t) alloc + HEAP_ALLOC_HEADER_BYTES + alloc->usableWords * sizeof(uintptr_t));
    if ((uintptr_t) nextAlloc >= ((uintptr_t) page) + HEAP_PAGE_HEADER_BYTES + page->usableWords * sizeof(uintptr_t)) return nullptr;
    return nextAlloc;
}

void* getDataPtr(HeapAlloc *alloc) {
    auto allocPtr = (void*) ((uintptr_t) alloc + HEAP_ALLOC_HEADER_BYTES);
    return allocPtr;
}

struct tryAllocateResp {void* dataPtr; HeapAlloc* hint;};

// https://github.com/jedisct1/libsodium/blob/be58b2e6664389d9c7993b55291402934b43b3ca/src/libsodium/sodium/utils.c#L78:L101
inline void memzero(void *data, size_t n) {
    volatile auto ptr = (uintptr_t*) data;
    for (int i = 0; i < n; i++) ptr[i] = 0;
}

tryAllocateResp tryAllocateInAlloc(HeapPage *heapPage, HeapAlloc *alloc, Type *type) {
    auto requiredAllocWords = type->requiredWords;

    tryAllocateResp resp {nullptr, nullptr};
    // if the allocation is big enough on its own, it's a perfect match
    if (alloc->usableWords >= requiredAllocWords) {

        // if the allocation is much bigger than the requested size, it's worth to split it
        // the split criterion is: the newly split allocation must have at least 10 usable words available
        bool worthSplitting = alloc->usableWords >= requiredAllocWords + HEAP_ALLOC_HEADER_WORDS + 0;

        if (worthSplitting) {
            auto oldUsableWords = alloc->usableWords;

            // adapting the 'old' big allocation
            alloc->usableWords = requiredAllocWords;
            alloc->type = type;
            alloc->colour = RUNTIME->gc->colour;

            // creating the new, smaller allocation
            auto splitAlloc = (HeapAlloc*) ((uintptr_t) alloc + HEAP_ALLOC_HEADER_BYTES + alloc->usableWords * sizeof(uintptr_t));
            splitAlloc->usableWords = oldUsableWords - HEAP_ALLOC_HEADER_WORDS - requiredAllocWords;
            splitAlloc->type = nullptr;
            splitAlloc->parentPage = alloc->parentPage;
            splitAlloc->colour = RUNTIME->gc->colour;
            resp.hint = splitAlloc;     // guaranteed to be free

        } else {

            // we use the allocation as is without changing its size
            alloc->type = type;
            alloc->colour = RUNTIME->gc->colour;

            HeapAlloc *nextAlloc = getNextAlloc(heapPage, alloc);
            if (nextAlloc && !nextAlloc->type) resp.hint = nextAlloc;    // guaranteed to be free
        }

        auto allocPtr = getDataPtr(alloc);
//        memset(allocPtr, 0, alloc->usableWords * sizeof(uintptr_t));    // zero-out memory
        memzero(allocPtr, alloc->usableWords);
        resp.dataPtr = allocPtr;
        return resp;
    }

    // here the allocation is free, but it's too small, so we're looking if the adjacent
    // allocations are also free hoping that we can merge them together
    // (and potentially split too if together they are too big)
    auto cumulativeUsableWords = alloc->usableWords;
    auto nextAlloc = getNextAlloc(heapPage, alloc);
    while (nextAlloc && nextAlloc->type == nullptr) {

        // both the free allocation header and its usable words count towards usable bytes
        // (if we want to merge the free allocations, their headers no longer matter and can be repurposed)
        cumulativeUsableWords += HEAP_ALLOC_HEADER_WORDS + nextAlloc->usableWords;
        if (cumulativeUsableWords < requiredAllocWords) {
            nextAlloc = getNextAlloc(heapPage, nextAlloc);
            continue;
        }

        // split if at least 10 additional words can be put into the new empty alloc
        bool worthSplitting = cumulativeUsableWords >= requiredAllocWords + HEAP_ALLOC_HEADER_WORDS + 0;
        if (worthSplitting) {
            // adapting the 'old' small allocation
            alloc->usableWords = requiredAllocWords;
            alloc->type = type;
            alloc->colour = RUNTIME->gc->colour;

            // splitting the last allocation
            auto splitAlloc = (HeapAlloc*) ((uintptr_t) alloc + HEAP_ALLOC_HEADER_BYTES + alloc->usableWords * sizeof(uintptr_t));
            splitAlloc->usableWords = cumulativeUsableWords - requiredAllocWords;
            splitAlloc->type = nullptr;
            splitAlloc->parentPage = alloc->parentPage;
            splitAlloc->colour = RUNTIME->gc->colour;

            resp.hint = splitAlloc;
        } else {

            // we merge the allocations without splitting
            alloc->usableWords = cumulativeUsableWords;
            alloc->type = type;
            alloc->colour = RUNTIME->gc->colour;

            if (nextAlloc && !nextAlloc->type) resp.hint = nextAlloc;    // guaranteed to be free
        }

        auto allocPtr = getDataPtr(alloc);
//        memset(allocPtr, 0, alloc->usableWords * sizeof(uintptr_t));    // zero-out memory
        memzero(allocPtr, alloc->usableWords);    // zero-out memory
        resp.dataPtr = allocPtr;
        return resp;
    }

    return resp;
}

void* tryAllocate(HeapPage *heapPage, Type *type, bool allowSpecialPurpose = false) {
    auto requiredAllocWords = type->requiredWords;

    // if the page is a single-purpose page, or it isn't, but it's too small, skip it
    if ((heapPage->isSinglePurpose && !allowSpecialPurpose) || heapPage->usableWords - HEAP_ALLOC_HEADER_WORDS < requiredAllocWords) {
        return nullptr;
    }

    HeapAlloc *alloc;

    alloc = heapPage->lastFreeAlloc;
    if (alloc) {
        auto resp = tryAllocateInAlloc(heapPage, alloc, type);
        if (resp.dataPtr) {
            heapPage->lastFreeAlloc = resp.hint;
            return resp.dataPtr;
        }
    }

    alloc = heapPage->middleFreeAlloc;
    if (alloc) {
        auto resp = tryAllocateInAlloc(heapPage, alloc, type);
        if (resp.dataPtr) {
            heapPage->middleFreeAlloc = resp.hint;
            return resp.dataPtr;
        }
    }

    return nullptr;
}

HeapPage *createNewHeapPage(size_t minUsablePageWords = HEAP_PAGE_SIZE_WORDS) {
//    std::cout << "New heap page!" << std::endl;
    // not enough space in any of the pages
    // create new page
    auto pageUsableWords = HEAP_PAGE_SIZE_WORDS;
    auto isSinglePurpose = false;
    if (minUsablePageWords > HEAP_PAGE_SIZE_WORDS) {
        pageUsableWords = minUsablePageWords;
        isSinglePurpose = true;
    }

    auto *newPage = (HeapPage *) new(std::nothrow) uintptr_t[HEAP_PAGE_HEADER_WORDS + pageUsableWords];

    if (!newPage) {
        // FIXME: OutOfMemoryError
        printf("OOME!\n");
        exit(15);
    }

    newPage->nextPage = nullptr;
    newPage->usableWords = pageUsableWords;
    newPage->isSinglePurpose = isSinglePurpose;

    auto *newPageAlloc = getNextAlloc(newPage);
    newPageAlloc->usableWords = pageUsableWords - HEAP_ALLOC_HEADER_WORDS;
    newPageAlloc->type = nullptr;
    newPageAlloc->parentPage = newPage;
    newPageAlloc->colour = RUNTIME->gc->colour;

    newPage->middleFreeAlloc = nullptr;
    newPage->lastFreeAlloc = newPageAlloc;
    return newPage;
}

void *Allocator::alloc(Type *type, size_t idx) {
    void* dataPtr = tryAllocate(lastPage, type);

    if (!dataPtr) {
        auto *newPage = createNewHeapPage(HEAP_ALLOC_HEADER_WORDS + type->requiredWords);
        dataPtr = tryAllocate(newPage, type, true);

        if (!dataPtr) {
            std::cerr << "Failed to allocate on a fresh heap page!" << std::endl;
            exit(1);
        }

        // TODO does this have to happen atomically?
        // probably no as long as no traversal through the page chain assumes that
        // the traversal should carry on until `heapPage == thread->lastPage`
        // (just check heapPage->nextPage == nullptr instead)
        lastPage->nextPage = newPage;
        lastPage = newPage;
    }

    this->pointerStack[idx] = (uintptr_t) dataPtr;

    return dataPtr;
}

void Allocator::dealloc(void *dataPtr) {
    if (dataPtr == nullptr) return;
    HeapAlloc *alloc = (HeapAlloc*) ((uintptr_t) dataPtr - HEAP_ALLOC_HEADER_BYTES);
    alloc->type = nullptr;
    alloc->parentPage->middleFreeAlloc = alloc;
}

void markPtrRecursive(uintptr_t dataPtr, std::queue<uintptr_t> &queue, unsigned short recursionLimit = 100) {
    if (dataPtr == 0) return;

    HeapAlloc *alloc = (HeapAlloc*) (dataPtr - HEAP_ALLOC_HEADER_BYTES);
    if (alloc->colour == RUNTIME->gc->colour) return;

    alloc->colour = RUNTIME->gc->colour;
    alloc->parentPage->colour = alloc->colour;

    auto type = alloc->type;

    if (!type) return;

    auto pointersCount = type->pointersCount;

    for (int i = 0; i < pointersCount; i++) {
        auto fieldDataPtr = *((uintptr_t *) dataPtr + i);
        if (fieldDataPtr == 0) continue;

        if (recursionLimit == 0) {
            queue.push(fieldDataPtr);
        } else {
            markPtrRecursive(fieldDataPtr, queue, recursionLimit - 1);
        }
    }
}

void gcMarkThread(ThreadRuntime *thread) {
    std::queue<uintptr_t> pointerQueue;

    for (auto &pointer : thread->allocator.pointerStack) {
        if (pointer == 0) continue;
        markPtrRecursive(pointer, pointerQueue);
    }

    while (!pointerQueue.empty()) {
        uintptr_t dataPtr = pointerQueue.front();
        markPtrRecursive(dataPtr, pointerQueue);
        pointerQueue.pop();
    }
}

void gcSweepThread(ThreadRuntime *thread, HeapPage *endPage) {
    auto *currentPage = thread->allocator.firstPage;
    auto *previousPage = (HeapPage*) nullptr;

    auto currentColour = RUNTIME->gc->colour;

    int freedPages = 0;

    while ((currentPage != nullptr) & (currentPage != endPage)) {

        bool pageMustLive = currentPage->colour == currentColour;

        if (pageMustLive) {
            previousPage = currentPage;
            currentPage = currentPage->nextPage;
            continue;
        }

        // from here on we know we want to get rid of the page
        // the next page should be connected to the chain in place of the to-be-deleted page
        auto *nextPage = currentPage->nextPage;

        if (currentPage == thread->allocator.firstPage) {
            // we are about to remove the first heap page
            // we can only do it as long as there are more pages
            if (!nextPage) break;

            thread->allocator.firstPage = nextPage;
            previousPage = nullptr;
        } else {
            // ordinary situation, lastPage is just another page in the chain
            previousPage->nextPage = nextPage;
        }

        delete[] currentPage; // pages are allocated as uintptr_t[] so they need to be deallocated as arrays

//        std::cout << "Removed heap page!" << std::endl;

        freedPages += 1;

        currentPage = nextPage;
    }
}

void gcST() {
    RUNTIME->gc->gcMutex.lock();

    RUNTIME->gc->colour = RUNTIME->gc->colour == Colour::Blue ? Colour::Green : Colour::Blue;

    auto start = std::chrono::steady_clock::now();

    auto *thread = RUNTIME->mainThread;
    while (thread) {
        gcMarkThread(thread);
        thread = thread->nextRuntime;
    }

    thread = RUNTIME->mainThread;
    while (thread) {
        gcSweepThread(thread, thread->allocator.lastPage);
        thread = thread->nextRuntime;
    }

    RUNTIME->gc->gcMutex.unlock();

//    std::cout << "GC took (ms)=" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << std::endl;
}

void gc() {
    RUNTIME->gc->gcMutex.lock();

    RUNTIME->gc->colour = RUNTIME->gc->colour == Colour::Blue ? Colour::Green : Colour::Blue;

    std::vector<std::thread *> workerThreads;
    workerThreads.reserve(16);

    auto *thread = RUNTIME->mainThread;
    while (thread) {
        workerThreads.push_back(new std::thread(gcMarkThread, thread));
        thread = thread->nextRuntime;
    }

    for (auto *workerThread : workerThreads) {
        workerThread->join();
        delete workerThread;
    }

    workerThreads.clear();

    thread = RUNTIME->mainThread;
    while (thread) {
        workerThreads.push_back(new std::thread(gcSweepThread, thread, thread->allocator.lastPage));
        thread = thread->nextRuntime;
    }

    for (auto *workerThread : workerThreads) {
        workerThread->join();
        delete workerThread;
    }

    RUNTIME->gc->gcMutex.unlock();
}

void addThread() {

}

void removeThread() {

}

void gcThreadTask() {
    while (RUNTIME->mainThread->isActive) {
        auto start = std::chrono::steady_clock::now();
        gcST();
//        std::cout << "GC took (ms)=" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << std::endl;
        timespec t1;
        t1.tv_sec = 0;
        t1.tv_nsec = 1000 * 1000 * 100; // always keep it less than 1s, otherwise it does not sleep
        nanosleep(&t1, &t1);
    }
}

ThreadRuntime* initRuntime() {
    RUNTIME = new Runtime {
            .gc = new GC {
//                    .gcMutex = std::mutex(),
                    .colour = Colour::Blue,
            },
//            .threadMutex = std::mutex(),
            .mainThread = nullptr,
    };

    RUNTIME->mainThread = new ThreadRuntime{
            .nextRuntime = nullptr,
            .allocator = Allocator(),
            .isActive = true,
    };

    RUNTIME->mainThread->allocator.lastPage = RUNTIME->mainThread->allocator.firstPage;
    RUNTIME->gc->gcThread = new std::thread(gcThreadTask);

    return RUNTIME->mainThread;
}

Allocator::Allocator() {
    this->firstPage = createNewHeapPage();
    this->lastPage = this->firstPage;
}

void shutdownRuntime() {
    RUNTIME->mainThread->isActive = false;
    RUNTIME->gc->gcThread->join();

    auto *heapPage = RUNTIME->mainThread->allocator.firstPage;

    while (heapPage) {
        // FIXME this is wrong as other threads might store data here too
        auto *nextPage = heapPage->nextPage;
        delete[] heapPage;
        heapPage = nextPage;
    }

    delete RUNTIME->gc->gcThread;
    delete RUNTIME->gc;
    delete RUNTIME->mainThread;
    delete RUNTIME;
}

void printHeap(ThreadRuntime *thread) {
    unsigned int pageCount = 0;
    auto *page = thread->allocator.firstPage;

    while (page) {
        std::cout << "Heap page " << pageCount << std::endl;

        auto *alloc = getNextAlloc(page);

        size_t allocationCount = 0;

        size_t totalPageWordsUsed = 0;

        while (alloc) {
            std::cout << "Allocation " << allocationCount << " (type=" << alloc->type << " , words=" << alloc->usableWords << ")" << std::endl;
            allocationCount++;

            totalPageWordsUsed += (HEAP_ALLOC_HEADER_WORDS + alloc->usableWords) * (alloc->type != nullptr);

            alloc = getNextAlloc(page, alloc);
        }

        std::cout << "Total words used (" << totalPageWordsUsed << " / " << page->usableWords << ")" << std::endl << std::endl;

        pageCount++;
        page = page->nextPage;
    }
}

void printHeapSummary(ThreadRuntime *thread) {
    auto *page = thread->allocator.firstPage;

    unsigned int pageCount = 0;
    size_t allocationCount = 0;
    size_t totalPageWordsUsed = 0;

    while (page) {

        auto *alloc = getNextAlloc(page);

        while (alloc) {

            if (alloc->type != nullptr) {
                allocationCount++;

                totalPageWordsUsed += (HEAP_ALLOC_HEADER_WORDS + alloc->usableWords) * (alloc->type != nullptr);

            }

            alloc = getNextAlloc(page, alloc);
        }

        pageCount++;
        page = page->nextPage;
    }
}
