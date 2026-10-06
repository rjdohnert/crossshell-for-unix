#pragma once

#include "yacc_config.hpp"
#include "yacc.hpp"

class BisonStructuredBuffer : public std::streambuf {
    std::streambuf* target_;
    Config::OutputFormat format_;
    std::string pending_;
    bool first_ = true;
    void write(const std::string& text);
    void emit();
public:
    BisonStructuredBuffer(std::streambuf* target, Config::OutputFormat format);
    ~BisonStructuredBuffer() override;
    int_type overflow(int_type ch) override;
    int sync() override;
};
