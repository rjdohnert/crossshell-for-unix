#pragma once

#include "yacc.hpp"

class BisonPipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit BisonPipeBuffer(FILE* file);
    int_type overflow(int_type ch) override;
    int sync() override;
};
