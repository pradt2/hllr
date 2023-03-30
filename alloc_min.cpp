#include <vector>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <list>

#define HEAP_PAGE_SIZE_WORDS 128000U
#define SMALL_ALLOC_WORDS (32 + 1)
#define HEAP_PAGE_MIN_FREE (HEAP_PAGE_SIZE_WORDS - SMALL_ALLOC_WORDS)

class Allocator {
    std::list<uintptr_t*> pages;
    std::size_t currPageWordsUsed;

    void add_page() {
        this->pages.push_back((std::uintptr_t*) calloc(sizeof(std::uintptr_t), HEAP_PAGE_SIZE_WORDS));
        this->currPageWordsUsed = 0;
    }

public:
    explicit Allocator() {
//        this->pages.reserve(128);
        this->add_page();
    }

    void* alloc_small(uintptr_t type, size_t size) {
        uintptr_t* lastPage = pages.back();
        lastPage[currPageWordsUsed] = (uintptr_t) lastPage;
        lastPage[currPageWordsUsed + 1] = type;
        uintptr_t* alloc = lastPage + currPageWordsUsed + 2;
        currPageWordsUsed += 2 + size;

        if (currPageWordsUsed <= HEAP_PAGE_MIN_FREE) [[likely]] {
            return alloc;
        } else [[unlikely]] {
            this->add_page();
            return alloc;
        }
    }

    void wipe() {
        for (auto *page : this->pages) {
            free(page);
        }
        this->pages.clear();
//        this->pages.reserve(128);
        this->add_page();
    }
};

const auto size = 1024;
const long iters = 1024 * 256 * 3;

int main() {
    Allocator alloc = Allocator();

    auto **nodes = new uintptr_t*[iters];

    auto start = std::chrono::steady_clock::now();

    for (long iter = 0; iter < size; iter++) {
        for (int i = 0; i < iters; i++) {
//            alloc.dealloc(nodes[i]);
            nodes[iter] =
                    (uintptr_t* ) alloc.alloc_small(1, 0);
        }
        if (iter % 2 == 0) alloc.wipe();
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    std::cout << ms << std::endl;

    std::cout << "Allocs per sec (in millions) " << ((size * iters) / ms * 1000) / 1000000.0 << std::endl;
}
