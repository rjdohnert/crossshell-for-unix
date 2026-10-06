#pragma once

#include "ulimit.hpp"

class WidePipeBuffer : public std::wstreambuf {
public:
    explicit WidePipeBuffer(FILE* f);
    int_type overflow(int_type c) override;
    int sync() override;
private:
    FILE* file_;
    wchar_t buffer_[1024];
};
