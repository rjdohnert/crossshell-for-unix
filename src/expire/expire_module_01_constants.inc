static int g_expire_format = 0;
static std::wstring g_expire_pipe;

static std::string expire_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <map>

//
// CONSTANTS & SIGNAL DEFINITIONS
// 

constexpr DWORD EXIT_TIMEOUT_EXPIRED  = 124;
constexpr DWORD EXIT_INTERNAL_ERROR   = 125;
constexpr DWORD EXIT_CANNOT_INVOKE    = 126;
constexpr DWORD EXIT_CMD_NOT_FOUND    = 127;
constexpr DWORD EXIT_FORCE_KILLED     = 137; // 128 + 9 (SIGKILL)

enum UnixSignal {
    SIG_HUP  = 1,
    SIG_INT  = 2,
    SIG_QUIT = 3,
    SIG_KILL = 9,
    SIG_TERM = 15
};

// 
// RAII HANDLE WRAPPER
// 

class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~ScopedHandle() { close(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { close(); h_ = other.h_; other.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    [[nodiscard]] HANDLE get() const { return h_; }
    void close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }
    [[nodiscard]] bool is_valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }
};

