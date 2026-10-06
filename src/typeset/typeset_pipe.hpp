#pragma once

#include "typeset.hpp"

class TypesetPipe : public std::wstreambuf {
private:
    FILE* m_file;
    wchar_t m_buffer[1024];

public:
    explicit TypesetPipe(FILE* f);

    int_type overflow(int_type c) override;

    int sync() override;
};
