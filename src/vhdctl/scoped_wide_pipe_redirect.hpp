#pragma once

#include "vhdctl.hpp"
#include "wide_pipe_buffer.hpp"

class ScopedWidePipeRedirect {
public:
    explicit ScopedWidePipeRedirect(const std::wstring& pipeCommand);

    ~ScopedWidePipeRedirect();

    bool IsValid() const;

private:
    FILE* m_pipe = nullptr;
    std::wstreambuf* m_oldOutput = nullptr;
    std::unique_ptr<WidePipeBuffer> m_buffer;
};
