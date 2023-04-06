mod memset;

use std::alloc::{alloc, Layout};
use std::collections::LinkedList;
use std::env;
use std::mem::{align_of, size_of};
use std::ptr::{slice_from_raw_parts_mut, write_volatile};

const HEAP_PAGE_SIZE_WORDS: usize = 128000;

type WORD = usize;

unsafe fn layout(size: WORD) -> Layout {
    Layout::from_size_align_unchecked(size_of::<WORD>() * size, align_of::<WORD>())
}


struct Allocator {
    pages: LinkedList<*mut WORD>,
    curr_page_words_used: WORD,
}

impl Allocator {

    unsafe fn new() -> Allocator {
        let mut allocator = Allocator {
            pages: Default::default(),
            curr_page_words_used: 0,
        };

        allocator.add_page(HEAP_PAGE_SIZE_WORDS);

        allocator
    }

    unsafe fn add_page(&mut self, size: WORD) {
        let page_ptr: *mut WORD = std::mem::transmute(std::alloc::alloc(layout(size)));
        self.pages.push_back(page_ptr);

        // it seems performance beneficial to write to the newly allocated page
        // even if the value is not used later
        *page_ptr = size;

        if cfg!(feature = "page-size-tracking") {
            self.curr_page_words_used = 1;
        } else {
            self.curr_page_words_used = 0;
        }
    }

    unsafe fn alloc<const TYP: WORD, const SIZE: WORD>(&mut self) -> *const WORD {
        if self.curr_page_words_used > HEAP_PAGE_SIZE_WORDS - SIZE - 2 {
            self.add_page(HEAP_PAGE_SIZE_WORDS)
        }

        let last_page = *self.pages.back_mut().unwrap();

        *last_page.add(self.curr_page_words_used) = std::mem::transmute(last_page);
        *last_page.add(self.curr_page_words_used + 1) = TYP;

        let alloc = last_page.add(self.curr_page_words_used + 2);

        for offset in 0..SIZE {
            alloc.add(offset).write(0);
        }

        self.curr_page_words_used += 2 + SIZE;

        alloc
    }

    unsafe fn wipe(&mut self) {
        for page in self.pages.iter() {
            let size = if cfg!(feature = "page-size-tracking") {
                **page as usize
            } else {
                0
            };
            std::alloc::dealloc(std::mem::transmute(*page), layout(size));
        }
        self.pages.clear();
        self.add_page(HEAP_PAGE_SIZE_WORDS);
    }
}

const SIZE: WORD = 1024;
const ITERS: WORD = 1024 * 256 * 3;

fn main() {
    let size = env::args().len() * 2;

    unsafe {
        let mut alloc = Allocator::new();

        let start = std::time::Instant::now();

        for iter in 0..SIZE {
            for _i in 0..ITERS {
                alloc.alloc::<1, 0>();
            }
            if iter % 1 == 0 {
                alloc.wipe();
            }
        }

        let end = std::time::Instant::now();

        let ms = end.checked_duration_since(start).unwrap().as_millis() as WORD;

        println!("{}", ms);
        println!("Allocs per sec (in millions) {}", ((SIZE * ITERS) / ms * 1000) / 1000000);
    }
}
