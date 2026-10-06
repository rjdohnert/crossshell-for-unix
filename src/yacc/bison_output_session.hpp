#pragma once

#include "bison_pipe_buffer.hpp"
#include "bison_structured_buffer.hpp"
#include "yacc_config.hpp"
#include "yacc.hpp"

class BisonOutputSession {
    std::streambuf* old_;
    FILE* pipe_ = nullptr;
    BisonPipeBuffer* pipeBuffer_ = nullptr;
    BisonStructuredBuffer* structured_ = nullptr;
public:
    BisonOutputSession(Config::OutputFormat format, const std::string& command);
    ~BisonOutputSession();
};

// --- Helper Functions ---
