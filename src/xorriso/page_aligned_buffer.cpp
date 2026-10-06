#include "page_aligned_buffer.hpp"

PageAlignedBuffer::PageAlignedBuffer(size_t size) : size_(size) {
        ptr_ = VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

PageAlignedBuffer::~PageAlignedBuffer() {
        if (ptr_) VirtualFree(ptr_, 0, MEM_RELEASE);
    }

uint8_t* PageAlignedBuffer::data() const { return static_cast<uint8_t*>(ptr_); }

size_t PageAlignedBuffer::size() const { return size_; }

bool PageAlignedBuffer::isValid() const { return ptr_ != nullptr; }
