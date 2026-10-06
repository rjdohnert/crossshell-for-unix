#pragma once

#include "strace.hpp"

class WidePipeStreambuf : public std::wstreambuf {
public:
    explicit WidePipeStreambuf(FILE* pipe);
    ~WidePipeStreambuf() override;

protected:
    int_type overflow(int_type ch) override;
    int sync() override;

private:
    FILE* m_pipe = nullptr;
    wchar_t m_buffer[2048] = {};
};
