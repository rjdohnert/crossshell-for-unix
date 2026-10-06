#pragma once

#include "xorriso.hpp"

class PageAlignedBuffer {
    void* ptr_ = nullptr;
    size_t size_ = 0;
public:
    PageAlignedBuffer(size_t size);
    ~PageAlignedBuffer();
    uint8_t* data() const;
    size_t size() const;
    bool isValid() const;
};
