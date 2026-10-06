#include "scoped_wide_pipe_redirect.hpp"
#include "wide_pipe_buffer.hpp"

ScopedWidePipeRedirect::ScopedWidePipeRedirect(const std::wstring& pipeCommand) {
        if (!pipeCommand.empty()) {
            m_pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (m_pipe) {
                m_oldOutput = std::wcout.rdbuf();
                m_buffer = std::make_unique<WidePipeBuffer>(m_pipe);
                std::wcout.rdbuf(m_buffer.get());
            }
        }
    }

ScopedWidePipeRedirect::~ScopedWidePipeRedirect() {
        if (m_pipe) {
            std::wcout.rdbuf(m_oldOutput);
            m_buffer.reset();
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

bool ScopedWidePipeRedirect::IsValid() const { return m_pipe != nullptr; }
