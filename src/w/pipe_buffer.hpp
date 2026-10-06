#pragma once

#include "w.hpp"

enum class OutputFormat { Human, Json, Csv, Tsv, Table };

class PipeBuffer : public std::streambuf {
    FILE* file_; char buffer_[4096];
public:
    explicit PipeBuffer(FILE* file);
    int_type overflow(int_type ch) override;
    int sync() override;
};
