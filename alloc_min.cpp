#include <cstdlib>
#include <chrono>
#include <iostream>
#include <list>
#include <vector>
#include <cstring>

#define HEAP_PAGE_SIZE_WORDS 128000
#define SMALL_ALLOC_WORDS (32 + 1)
#define HEAP_PAGE_MIN_FREE (HEAP_PAGE_SIZE_WORDS - SMALL_ALLOC_WORDS)

class Allocator {
    std::list<uintptr_t*> pages;
    uintptr_t *lastPage;
    long currPageWordsUsed;

    void add_page(unsigned long size = HEAP_PAGE_SIZE_WORDS) {
        this->pages.push_back(new uintptr_t[size]);
        this->lastPage = this->pages.back();
        this->currPageWordsUsed = 0;
    }

public:
    explicit Allocator() {
        this->add_page();
    }

    void* alloc_small(uintptr_t type, long size) {
        if (currPageWordsUsed > HEAP_PAGE_MIN_FREE) {
            this->add_page();
        }

        lastPage[currPageWordsUsed] = (uintptr_t) lastPage;
        lastPage[currPageWordsUsed + 1] = type;
        uintptr_t* alloc = lastPage + currPageWordsUsed + 2;
        currPageWordsUsed += 2 + size;

        std::memset(alloc, 0, size); // faster than the libsodium method

        return alloc;
    }

    void *alloc_big(uintptr_t type, long size) {
        if (HEAP_PAGE_SIZE_WORDS - 2 - this->currPageWordsUsed >= size)  {
        } else {
            this->add_page();
        }

        lastPage[currPageWordsUsed] = (uintptr_t) lastPage;
        lastPage[currPageWordsUsed + 1] = type;
        uintptr_t* alloc = lastPage + currPageWordsUsed + 2;
        currPageWordsUsed += 2 + size;

        std::memset(alloc, 0, size);

        return alloc;
    }

    void wipe() {
        for (auto *page : this->pages) {
            delete[] page;
        }
        this->pages.clear();
        this->add_page();
    }
};

const auto size = 1024;
const long iters = 1024 * 256 * 3;

int main() {
    Allocator alloc = Allocator();

    auto **nodes = new uintptr_t*[iters];

    auto start = std::chrono::high_resolution_clock::now();

    for (long iter = 0; iter < size; iter++) {
        for (int i = 0; i < iters; i++) {
//            nodes[i] =
                    (uintptr_t* ) alloc.alloc_small(1, 2);
        }
        if (iter % 2 == 0) alloc.wipe();
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((size * iters) / ms * 1000) / 1000000.0 << std::endl;
}
