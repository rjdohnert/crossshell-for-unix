#pragma once

#include "pipe_buffer.hpp"
#include "string_encoding.hpp"
#include "w.hpp"

class PipeSession {
    std::streambuf* old_; FILE* file_ = nullptr; PipeBuffer* buffer_ = nullptr;
public:
    explicit PipeSession(const std::string& command);
    ~PipeSession();
};
