#pragma once

#include "json.hpp"
#include "registryctl.hpp"

enum class RegistryOutputFormat { Human, Json, Csv, Table };

class RegistryPipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit RegistryPipeBuffer(FILE* file);
    int_type overflow(int_type ch) override;
    int sync() override;
};
