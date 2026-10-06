#pragma once

#include "wc.hpp"

class PipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit PipeBuffer(FILE* file);
    int_type overflow(int_type ch) override;
    int sync() override;
};
