#pragma once

#include "pthctl.hpp"

class PthOutputBuffer : public std::wstreambuf {
    std::wstreambuf* target_;
    OutputFormat format_;
    std::wstring pending_;
    bool first_ = true;
    void write(const std::wstring& value);
    void emit();
protected:
    int_type overflow(int_type ch) override;
    int sync() override;
public:
    PthOutputBuffer(std::wstreambuf* target, OutputFormat format);
    ~PthOutputBuffer() override = default;
    void setTarget(std::wstreambuf* target);
    void start();
    void finish();
};

class PthPipeBuffer : public std::wstreambuf {
    FILE* file_;
    wchar_t buffer_[1024];
public:
    explicit PthPipeBuffer(FILE* file);
    int_type overflow(int_type ch) override;
    int sync() override;
};

class PthOutputSession {
    std::wstreambuf* old_;
    PthOutputBuffer buffer_;
    FILE* pipe_ = nullptr;
    PthPipeBuffer* pipeBuffer_ = nullptr;
public:
    PthOutputSession(OutputFormat format, const std::wstring& command);
    ~PthOutputSession();
};
