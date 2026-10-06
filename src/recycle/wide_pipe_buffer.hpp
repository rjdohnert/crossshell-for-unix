#pragma once

#include "recycle.hpp"

enum class OutputFormat { Human, Json, Csv, Table };

class WidePipeBuffer : public std::wstreambuf {
private:
    FILE* m_file{nullptr};
    wchar_t m_buffer[2048];

public:
    explicit WidePipeBuffer(FILE* file);

    int_type overflow(int_type ch) override;

    int sync() override;
};
