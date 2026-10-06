#pragma once

#include "strace.hpp"
#include "wide_pipe_streambuf.hpp"

class OutputRedirectionGuard {
public:
    OutputRedirectionGuard(const std::wstring& outputPath, const std::wstring& pipeCommand);
    ~OutputRedirectionGuard();

    OutputRedirectionGuard(const OutputRedirectionGuard&) = delete;
    OutputRedirectionGuard& operator=(const OutputRedirectionGuard&) = delete;

private:
    std::wofstream m_traceFile;
    FILE* m_pipe = nullptr;
    std::unique_ptr<WidePipeStreambuf> m_pipeBuffer;
    std::wstreambuf* m_oldBuffer = nullptr;
    bool m_isFileRedirected = false;
    bool m_isPipeRedirected = false;
};
