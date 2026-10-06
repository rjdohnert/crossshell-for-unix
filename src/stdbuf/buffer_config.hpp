#pragma once

#include "stdbuf.hpp"

struct BufferConfig {
    BufferMode mode{BufferMode::LineBuffered};
    size_t size{4096};

    static BufferConfig parse(const std::wstring& str);
};
