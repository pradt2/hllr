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

void* tryAllocate(HeapPage *heapPage, Type *type, bool allowSpecialPurpose = false) {
    auto requiredAllocWords = type->requiredWords;

    // if the page is a single-purpose page, or it isn't, but it's too small, skip it
    if ((heapPage->isSinglePurpose && !allowSpecialPurpose) || heapPage->usableWords - HEAP_ALLOC_HEADER_WORDS < requiredAllocWords) {
        return nullptr;
    }

    auto *alloc = heapPage->freeAllocHint;
    if (!alloc) alloc = getNextAlloc(heapPage);

    while (alloc) {

        // if the allocation isn't free, skip it
        if (alloc->type != nullptr) {
            alloc = getNextAlloc(heapPage, alloc);
            continue;
        }

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
                splitAlloc->colour = RUNTIME->gc->colour;

                heapPage->freeAllocHint = splitAlloc;
            } else {

                // we use the allocation as is without changing its size
                alloc->type = type;
                alloc->colour = RUNTIME->gc->colour;

                heapPage->freeAllocHint = getNextAlloc(heapPage, alloc);
            }

            auto allocPtr = getDataPtr(alloc);
            memset(allocPtr, 0, alloc->usableWords * sizeof(uintptr_t));    // zero-out memory
            return allocPtr;
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
                splitAlloc->colour = RUNTIME->gc->colour;

                heapPage->freeAllocHint = splitAlloc;
            } else {

                // we merge the allocations without splitting
                alloc->usableWords = cumulativeUsableWords;
                alloc->type = type;
                alloc->colour = RUNTIME->gc->colour;

                heapPage->freeAllocHint = getNextAlloc(heapPage, alloc);
            }

            auto allocPtr = getDataPtr(alloc);
            memset(allocPtr, 0, alloc->usableWords * sizeof(uintptr_t));    // zero-out memory
            return allocPtr;
        }

        // next alloc exists, but is not empty, we should resume the search
        if (nextAlloc) {
            alloc = getNextAlloc(heapPage, nextAlloc);
            continue;
        }

        // the next alloc does not exist, which means we've reached the end of the page
        return nullptr;
    }

    return nullptr;
}

HeapPage *createNewHeapPage(size_t minUsablePageWords = HEAP_PAGE_SIZE_WORDS) {
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
    newPageAlloc->colour = RUNTIME->gc->colour;

    newPage->freeAllocHint = newPageAlloc;
    return newPage;
}

void *alloc(Thread *thread, Type *type) {

    auto *heapPage = thread->lastPage;

    void* dataPtr = tryAllocate(heapPage, type);
    if (dataPtr) return dataPtr;

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
    thread->lastPage = newPage;
    heapPage->nextPage = newPage;

    return dataPtr;
}

void markPtrRecursive(uintptr_t dataPtr, std::queue<uintptr_t> &queue, unsigned short recursionLimit = 100) {
    if (dataPtr == 0) return;

    HeapAlloc *alloc = (HeapAlloc*) (dataPtr - HEAP_ALLOC_HEADER_BYTES);
    if (alloc->colour == RUNTIME->gc->colour) return;
    alloc->colour = RUNTIME->gc->colour;

    for (int i = 0; i < alloc->type->pointersCount; i++) {
        auto fieldDataPtr = *((uintptr_t *) dataPtr + i);
        if (fieldDataPtr == 0) continue;

        if (recursionLimit == 0) {
            queue.push(fieldDataPtr);
        } else {
            markPtrRecursive(fieldDataPtr, queue, recursionLimit - 1);
        }
    }
}

void gcMarkThread(Thread *thread) {
    std::queue<uintptr_t> pointerQueue;

    auto *pointerPage = thread->pointerPage;
    while (pointerPage) {
        size_t pointerCount = pointerPage->pointerCount;
        auto firstPointer = (uintptr_t *) ((uintptr_t) pointerPage + POINTER_PAGE_HEADER_BYTES);
        for (int i = 0; i < pointerCount; i++) {
            uintptr_t dataPtr = *(firstPointer + i);
            if (dataPtr == 0) continue;
            markPtrRecursive(dataPtr, pointerQueue);
        }
        pointerPage = pointerPage->nextPage;
    }

    while (!pointerQueue.empty()) {
        uintptr_t dataPtr = pointerQueue.front();
        markPtrRecursive(dataPtr, pointerQueue);
        pointerQueue.pop();
    }
}

void gcSweepThread(Thread *thread, HeapPage *endPage) {
    auto *currentPage = thread->heapPage;
    auto *previousPage = (HeapPage*) nullptr;

    while ((currentPage != nullptr) & (currentPage != endPage)) {
        auto *alloc = getNextAlloc(currentPage);

        bool pageMustLive = false;

        while (alloc) {
            if (alloc->type == nullptr) {
                // allocation is free, so we don't care about the colour
            } else if (alloc->colour != RUNTIME->gc->colour) {
                // allocation hasn't been marked
                // which means it has no reason to live, thus we can free it
                alloc->type = nullptr;
            } else {
                // allocation isn't free, and it is marked by the mark algorithm,
                // so it must live, and thus so must the entire page
                pageMustLive = true;
            }
            alloc = getNextAlloc(currentPage, alloc);
        }

        if (pageMustLive) {
            previousPage = currentPage;
            currentPage = currentPage->nextPage;
            continue;
        }

        // from here on we know we want to get rid of the page
        // the next page should be connected to the chain in place of the to-be-deleted page
        auto *nextPage = currentPage->nextPage;

        if (currentPage == thread->heapPage) {
            // we are about to remove the first heap page
            // we can only do it as long as there are more pages
            if (!nextPage) break;

            thread->heapPage = nextPage;
            previousPage = nullptr;
        } else {
            // ordinary situation, lastPage is just another page in the chain
            previousPage->nextPage = nextPage;
        }

        delete[] currentPage; // pages are allocated as uintptr_t[] so they need to be deallocated as arrays

        currentPage = nextPage;
    }
}

void gc() {
    RUNTIME->gc->gcMutex.lock();

    RUNTIME->gc->colour = RUNTIME->gc->colour == Colour::Blue ? Colour::Green : Colour::Blue;

    std::vector<std::thread *> workerThreads;
    workerThreads.reserve(16);

    auto *thread = RUNTIME->mainThread;
    while (thread) {
        workerThreads.push_back(new std::thread(gcMarkThread, thread));
        gcMarkThread(thread);
        thread = thread->nextThread;
    }

    for (auto *workerThread : workerThreads) {
        workerThread->join();
        delete workerThread;
    }

    workerThreads.clear();

    thread = RUNTIME->mainThread;
    while (thread) {
        workerThreads.push_back(new std::thread(gcSweepThread, thread, thread->lastPage));
        thread = thread->nextThread;
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
        gc();
        std::cout << "GC took (ms)=" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << std::endl;
        timespec t1;
        t1.tv_sec = 0;
        t1.tv_nsec = 1000 * 1000 * 50; // 100ms
        nanosleep(&t1, &t1);
    }
}

Thread* initRuntime(PointerPage *pointerPage) {
    RUNTIME = new Runtime {
            .gc = new GC {
                    .gcMutex = std::mutex(),
                    .colour = Colour::Blue,
            },
            .threadMutex = std::mutex(),
            .mainThread = nullptr,
    };

    RUNTIME->mainThread = new Thread {
            .heapPage = createNewHeapPage(),
            .pointerPage = pointerPage,
            .nextThread = nullptr,
            .isActive = true,
    };

    RUNTIME->mainThread->lastPage = RUNTIME->mainThread->heapPage;
    RUNTIME->gc->gcThread = new std::thread(gcThreadTask);

    return RUNTIME->mainThread;
}

void shutdownRuntime() {
    RUNTIME->mainThread->isActive = false;
    RUNTIME->gc->gcThread->join();

    auto *heapPage = RUNTIME->mainThread->heapPage;

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

void printHeap(Thread *thread) {
    unsigned int pageCount = 0;
    auto *page = thread->heapPage;

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

void printHeapSummary(Thread *thread) {
    auto *page = thread->heapPage;

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
