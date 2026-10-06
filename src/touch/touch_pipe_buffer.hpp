#pragma once

#include "touch.hpp"

class TouchPipeBuffer : public std::wstreambuf {
private:
    FILE* m_file;
    wchar_t m_buffer[1024];

public:
    explicit TouchPipeBuffer(FILE* file);

    int_type overflow(int_type ch) override;

    int sync() override;
};
