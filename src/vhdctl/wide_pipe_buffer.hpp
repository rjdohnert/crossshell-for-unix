#pragma once

#include "vhdctl.hpp"

class WidePipeBuffer : public std::wstreambuf {
    FILE* pipe_;
    wchar_t buffer_[2048];
protected:
    int_type overflow(int_type ch) override;
    int sync() override;
public:
    explicit WidePipeBuffer(FILE* pipe);
    ~WidePipeBuffer() override;
};
