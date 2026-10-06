#pragma once

#include "registry_line_formatter.hpp"
#include "registry_pipe_buffer.hpp"
#include "registryctl.hpp"

class RegistryOutputSession {
    std::streambuf* old_;
    FILE* pipe_ = nullptr;
    RegistryPipeBuffer* pipeBuffer_ = nullptr;
    RegistryLineFormatter* formatter_ = nullptr;
public:
    RegistryOutputSession(RegistryOutputFormat format, const std::string& command);
    ~RegistryOutputSession();
};
