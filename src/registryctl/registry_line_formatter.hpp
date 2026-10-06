#pragma once

#include "registry_pipe_buffer.hpp"
#include "registryctl.hpp"

class RegistryLineFormatter : public std::streambuf {
    std::streambuf* target_;
    RegistryOutputFormat format_;
    std::string pending_;
    void emit();
public:
    RegistryLineFormatter(std::streambuf* target, RegistryOutputFormat format);
    int_type overflow(int_type ch) override;
    int sync() override;
};
