#pragma once

#include "zcat.hpp"

class PipeBuffer : public std::streambuf {
private:
    FILE* m_file;
    char m_buffer[4096];

public:
    explicit PipeBuffer(FILE* file);

    int_type overflow(int_type ch) override;

    int sync() override;
};
