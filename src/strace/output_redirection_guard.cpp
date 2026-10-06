#include "output_redirection_guard.hpp"
#include "wide_pipe_streambuf.hpp"

OutputRedirectionGuard::OutputRedirectionGuard(const std::wstring& outputPath, const std::wstring& pipeCommand) {
    if (!outputPath.empty()) {
        m_traceFile.open(outputPath, std::ios::out | std::ios::binary);
        if (m_traceFile) {
            m_oldBuffer = std::wcout.rdbuf();
            std::wcout.rdbuf(m_traceFile.rdbuf());
            m_isFileRedirected = true;
        } else {
            std::wcerr << L"strace: failed to open output file: " << outputPath << std::endl;
        }
    } else if (!pipeCommand.empty()) {
        m_pipe = _wpopen(pipeCommand.c_str(), L"w");
        if (m_pipe) {
            m_oldBuffer = std::wcout.rdbuf();
            m_pipeBuffer = std::make_unique<WidePipeStreambuf>(m_pipe);
            std::wcout.rdbuf(m_pipeBuffer.get());
            m_isPipeRedirected = true;
        } else {
            std::wcerr << L"strace: failed to start pipe command" << std::endl;
        }
    }
}

OutputRedirectionGuard::~OutputRedirectionGuard() {
    if (m_oldBuffer != nullptr) {
        std::wcout.rdbuf(m_oldBuffer);
    }
    if (m_traceFile.is_open()) {
        m_traceFile.close();
    }
    m_pipeBuffer.reset();
    if (m_pipe != nullptr) {
        _pclose(m_pipe);
        m_pipe = nullptr;
    }
}
