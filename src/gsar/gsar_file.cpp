#define NOMINMAX

#include "gsar_file.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <streambuf>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
class FileBuffer final : public std::streambuf {
private:
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    int m_descriptor = -1;
#endif

protected:
    std::streamsize xsputn(const char* data, std::streamsize length) override {
        std::streamsize total = 0;
        while (total < length) {
#ifdef _WIN32
            const DWORD request = static_cast<DWORD>(
                (std::min<std::streamsize>)(length - total,
                    static_cast<std::streamsize>((std::numeric_limits<DWORD>::max)())));
            DWORD written = 0;
            if (!WriteFile(m_handle, data + total, request, &written, nullptr) ||
                written == 0) {
                break;
            }
            total += static_cast<std::streamsize>(written);
#else
            const ssize_t written = ::write(m_descriptor, data + total,
                                             static_cast<size_t>(length - total));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) break;
            total += static_cast<std::streamsize>(written);
#endif
        }
        return total;
    }

    int_type overflow(int_type character) override {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return traits_type::not_eof(character);
        }
        const char byte = traits_type::to_char_type(character);
        return xsputn(&byte, 1) == 1 ? character : traits_type::eof();
    }

    int sync() override {
#ifdef _WIN32
        return m_handle != INVALID_HANDLE_VALUE && FlushFileBuffers(m_handle) ? 0 : -1;
#else
        return m_descriptor >= 0 && ::fsync(m_descriptor) == 0 ? 0 : -1;
#endif
    }

public:
    ~FileBuffer() override {
        close(nullptr);
    }

#ifdef _WIN32
    void attach(HANDLE handle) {
        m_handle = handle;
    }
#else
    void attach(int descriptor) {
        m_descriptor = descriptor;
    }
#endif

    bool close(std::string* error) {
        bool succeeded = true;
        if (pubsync() != 0) {
            succeeded = false;
            if (error != nullptr) {
#ifdef _WIN32
                *error = std::system_error(static_cast<int>(GetLastError()),
                    std::system_category()).what();
#else
                *error = std::strerror(errno);
#endif
            }
        }
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) {
            if (!CloseHandle(m_handle)) {
                succeeded = false;
                if (error != nullptr && error->empty()) {
                    *error = std::system_error(static_cast<int>(GetLastError()),
                        std::system_category()).what();
                }
            }
            m_handle = INVALID_HANDLE_VALUE;
        }
#else
        if (m_descriptor >= 0) {
            if (::close(m_descriptor) != 0) {
                succeeded = false;
                if (error != nullptr && error->empty()) *error = std::strerror(errno);
            }
            m_descriptor = -1;
        }
#endif
        return succeeded;
    }
};

std::string SystemErrorMessage(const char* operation) {
#ifdef _WIN32
    const DWORD code = GetLastError();
    return std::string(operation) + ": " +
        std::system_error(static_cast<int>(code), std::system_category()).what();
#else
    return std::string(operation) + ": " + std::strerror(errno);
#endif
}
}

class AtomicOutput::Impl {
public:
    std::filesystem::path targetPath;
    std::filesystem::path temporaryPath;
    bool allowReplace = false;
    bool committed = false;
    FileBuffer fileBuffer;
    std::ostream output;

    Impl() : output(&fileBuffer) {}

    ~Impl() {
        fileBuffer.close(nullptr);
        if (!committed && !temporaryPath.empty()) {
            std::error_code error;
            std::filesystem::remove(temporaryPath, error);
        }
    }
};

AtomicOutput::AtomicOutput() : m_impl(std::make_unique<Impl>()) {}

AtomicOutput::~AtomicOutput() = default;

bool AtomicOutput::open(const std::filesystem::path& target, bool allowReplace,
                        std::filesystem::perms permissions, std::string& error) {
    if (target.empty()) {
        error = "output path is empty";
        return false;
    }

    m_impl->targetPath = target;
    m_impl->allowReplace = allowReplace;
    std::filesystem::path parent = target.parent_path();
    if (parent.empty()) parent = ".";

#ifdef _WIN32
    static std::atomic<unsigned long long> counter{0};
    for (unsigned int attempt = 0; attempt < 128; ++attempt) {
        const unsigned long long id = counter.fetch_add(1, std::memory_order_relaxed);
        const std::wstring name = target.filename().wstring() + L".gsar-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(id) + L".tmp";
        m_impl->temporaryPath = parent / name;
        HANDLE handle = CreateFileW(m_impl->temporaryPath.c_str(), GENERIC_WRITE, 0,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            m_impl->fileBuffer.attach(handle);
            return true;
        }
        const DWORD code = GetLastError();
        if (code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS) {
            error = SystemErrorMessage("cannot create temporary output");
            m_impl->temporaryPath.clear();
            return false;
        }
    }
    error = "could not allocate a unique temporary output file";
    m_impl->temporaryPath.clear();
    return false;
#else
    std::string pattern = (parent / (target.filename().string() + ".gsar-XXXXXX")).string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');
    const int descriptor = ::mkstemp(writablePattern.data());
    if (descriptor < 0) {
        error = SystemErrorMessage("cannot create temporary output");
        return false;
    }
    m_impl->temporaryPath = writablePattern.data();
    m_impl->fileBuffer.attach(descriptor);
    if (permissions != std::filesystem::perms::unknown) {
        const mode_t mode = static_cast<mode_t>(permissions & std::filesystem::perms::mask);
        if (::fchmod(descriptor, mode) != 0) {
            error = SystemErrorMessage("cannot preserve output permissions");
            m_impl->fileBuffer.close(nullptr);
            std::error_code removeError;
            std::filesystem::remove(m_impl->temporaryPath, removeError);
            m_impl->temporaryPath.clear();
            return false;
        }
    }
    return true;
#endif
}

std::ostream& AtomicOutput::stream() {
    return m_impl->output;
}

bool AtomicOutput::commit(std::string& error) {
    m_impl->output.flush();
    if (!m_impl->output) {
        error = "failed while flushing temporary output";
        return false;
    }
    if (!m_impl->fileBuffer.close(&error)) {
        if (error.empty()) error = "failed while closing temporary output";
        return false;
    }

#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(m_impl->targetPath.c_str());
    if (m_impl->allowReplace && attributes != INVALID_FILE_ATTRIBUTES) {
        if (!ReplaceFileW(m_impl->targetPath.c_str(), m_impl->temporaryPath.c_str(),
                          nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            error = SystemErrorMessage("cannot atomically replace output");
            return false;
        }
    } else {
        if (!MoveFileExW(m_impl->temporaryPath.c_str(), m_impl->targetPath.c_str(),
                         MOVEFILE_WRITE_THROUGH)) {
            error = SystemErrorMessage("cannot atomically install output");
            return false;
        }
    }
#else
    if (m_impl->allowReplace) {
        if (::rename(m_impl->temporaryPath.c_str(), m_impl->targetPath.c_str()) != 0) {
            error = SystemErrorMessage("cannot atomically replace output");
            return false;
        }
    } else {
        if (::link(m_impl->temporaryPath.c_str(), m_impl->targetPath.c_str()) != 0) {
            error = SystemErrorMessage("cannot atomically install output");
            return false;
        }
        if (::unlink(m_impl->temporaryPath.c_str()) != 0) {
            error = SystemErrorMessage("output installed but temporary link cleanup failed");
            return false;
        }
    }
#endif

    m_impl->committed = true;
    m_impl->temporaryPath.clear();
    return true;
}
