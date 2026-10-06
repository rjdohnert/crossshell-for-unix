#ifndef MAILX_HPP
#define MAILX_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = nullptr) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class MailOptions {
public:
    std::wstring subject;
    std::wstring from;
    std::wstring replyTo;
    std::wstring server = L"localhost";
    int port = 25;
    std::wstring user;
    std::wstring password;
    bool useSsl = false;
    bool isHtml = false;
    bool verbose = false;
    bool dryRun = false;
    int timeoutMs = 100000;
    std::vector<std::wstring> to;
    std::vector<std::wstring> cc;
    std::vector<std::wstring> bcc;
    std::vector<std::wstring> attachments;
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    std::wstring body;
    std::wstring bodyFile;
    bool showHelp = false;
    bool showVersion = false;

    static std::wstring Trim(const std::wstring& s);
    bool Parse(int argc, wchar_t* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};

#endif // MAILX_HPP
