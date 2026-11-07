#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>

enum class ConsoleWaitResult {
    InputReady,
    ShutdownRequested,
    Timeout,
    Error
};

class ConsoleSession {
public:
    ConsoleSession() = default;
    ~ConsoleSession();

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

    bool Initialize(std::string& error);
    void Restore() noexcept;
    ConsoleWaitResult WaitForInput(DWORD& error) const noexcept;
    bool ShutdownRequested() const noexcept;

    HANDLE Input() const noexcept { return input_; }
    HANDLE Output() const noexcept { return output_; }

private:
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    HANDLE shutdown_event_ = nullptr;
    DWORD input_mode_ = 0;
    DWORD output_mode_ = 0;
    bool input_mode_changed_ = false;
    bool output_mode_changed_ = false;
    bool handler_registered_ = false;
};
