use std::alloc::Layout;
use std::collections::LinkedList;
use std::ptr::slice_from_raw_parts_mut;

const HEAP_PAGE_SIZE_WORDS: usize = 128000;
const SMALL_ALLOC_WORDS: usize = 32 + 1;
const HEAP_PAGE_MIN_FREE: usize = HEAP_PAGE_SIZE_WORDS - SMALL_ALLOC_WORDS;

fn layout() -> Layout {
    Layout::array::<u64>(HEAP_PAGE_SIZE_WORDS).unwrap()
}


struct Allocator {
    pages: LinkedList<*mut u64>,
    curr_page_words_used: usize,
}

impl Allocator {

    unsafe fn new() -> Allocator {
        let mut allocator = Allocator {
            pages: Default::default(),
            curr_page_words_used: 0,
        };

        allocator.add_page();

        allocator
    }

    unsafe fn add_page(&mut self) {
        self.pages.push_back(std::mem::transmute(std::alloc::alloc(layout())));
        self.curr_page_words_used = 0;
    }

    unsafe fn alloc_small(&mut self, typ: u64, size: usize) -> *const u64 {
        if self.curr_page_words_used > HEAP_PAGE_MIN_FREE {
            self.add_page();
        }

        let last_page = *self.pages.back_mut().unwrap();

        *last_page.add(self.curr_page_words_used) = std::mem::transmute(last_page);
        *last_page.add(self.curr_page_words_used + 1) = typ;
        (*slice_from_raw_parts_mut(last_page.add(self.curr_page_words_used + 2), size)).fill(0x00);

        let alloc = last_page.add(2);

        self.curr_page_words_used += 2 + size;

        alloc
    }

    unsafe fn wipe(&mut self) {
        for page in self.pages.iter() {
            std::alloc::dealloc(std::mem::transmute(*page), layout());
        }
        self.pages.clear();
        self.add_page();
    }
}

const SIZE: u64 = 1024;
const ITERS: u64 = 1024 * 256 * 3;

fn main() {
    unsafe {
        let mut alloc = Allocator::new();

        let start = std::time::Instant::now();

        for iter in 0..SIZE {
            for _i in 0..ITERS {
                alloc.alloc_small(1, 2);
            }
            if iter % 1 == 0 {
                alloc.wipe();
            }
        }

        let end = std::time::Instant::now();

        let ms = end.checked_duration_since(start).unwrap().as_millis() as u64;

        println!("{}", ms);
        println!("Allocs per sec (in millions) {}", ((SIZE * ITERS) / ms * 1000) / 1000000);
    }
}
