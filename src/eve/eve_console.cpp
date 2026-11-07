#include "eve_console.h"

#include <atomic>

namespace {

std::atomic<bool> shutdown_requested{false};
std::atomic<HANDLE> shutdown_event{nullptr};

BOOL WINAPI HandleConsoleControl(DWORD control_type) {
    switch (control_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    {
        shutdown_requested.store(true, std::memory_order_relaxed);
        const HANDLE event = shutdown_event.load(std::memory_order_relaxed);
        if (event != nullptr) {
            SetEvent(event);
        }
        return TRUE;
    }
    default:
        return FALSE;
    }
}

void SetError(std::string& error, const char* action, DWORD code) {
    error = action;
    error += " (Windows error ";
    error += std::to_string(code);
    error += ")";
}

}

ConsoleSession::~ConsoleSession() {
    Restore();
}

bool ConsoleSession::Initialize(std::string& error) {
    input_ = GetStdHandle(STD_INPUT_HANDLE);
    output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input_ == nullptr || input_ == INVALID_HANDLE_VALUE ||
        output_ == nullptr || output_ == INVALID_HANDLE_VALUE) {
        error = "EVE requires a console for standard input and output";
        return false;
    }

    if (!GetConsoleMode(input_, &input_mode_)) {
        SetError(error, "Unable to read console input mode", GetLastError());
        return false;
    }
    if (!GetConsoleMode(output_, &output_mode_)) {
        SetError(error, "Unable to read console output mode", GetLastError());
        return false;
    }

    DWORD input_mode = input_mode_;
    input_mode &= ~ENABLE_QUICK_EDIT_MODE;
    input_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    input_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    if (!SetConsoleMode(input_, input_mode)) {
        SetError(error, "Unable to configure console input", GetLastError());
        return false;
    }
    input_mode_changed_ = true;

    if (!SetConsoleMode(output_, output_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        const DWORD code = GetLastError();
        Restore();
        SetError(error, "Unable to configure console output", code);
        return false;
    }
    output_mode_changed_ = true;

    shutdown_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (shutdown_event_ == nullptr) {
        const DWORD code = GetLastError();
        Restore();
        SetError(error, "Unable to create console shutdown event", code);
        return false;
    }

    shutdown_requested.store(false, std::memory_order_relaxed);
    shutdown_event.store(shutdown_event_, std::memory_order_relaxed);
    if (!SetConsoleCtrlHandler(HandleConsoleControl, TRUE)) {
        const DWORD code = GetLastError();
        shutdown_event.store(nullptr, std::memory_order_relaxed);
        Restore();
        SetError(error, "Unable to register console control handler", code);
        return false;
    }
    handler_registered_ = true;
    return true;
}

void ConsoleSession::Restore() noexcept {
    if (handler_registered_) {
        SetConsoleCtrlHandler(HandleConsoleControl, FALSE);
        handler_registered_ = false;
    }
    if (shutdown_event.load(std::memory_order_relaxed) == shutdown_event_) {
        shutdown_event.store(nullptr, std::memory_order_relaxed);
    }
    if (input_mode_changed_ && input_ != nullptr && input_ != INVALID_HANDLE_VALUE) {
        SetConsoleMode(input_, input_mode_);
        input_mode_changed_ = false;
    }
    if (output_mode_changed_ && output_ != nullptr && output_ != INVALID_HANDLE_VALUE) {
        SetConsoleMode(output_, output_mode_);
        output_mode_changed_ = false;
    }
    if (shutdown_event_ != nullptr) {
        CloseHandle(shutdown_event_);
        shutdown_event_ = nullptr;
    }
}

ConsoleWaitResult ConsoleSession::WaitForInput(DWORD& error) const noexcept {
    if (input_ == nullptr || input_ == INVALID_HANDLE_VALUE || shutdown_event_ == nullptr) {
        error = ERROR_INVALID_HANDLE;
        return ConsoleWaitResult::Error;
    }

    HANDLE handles[] = {input_, shutdown_event_};
    const DWORD result = WaitForMultipleObjects(2, handles, FALSE, 500);
    if (result == WAIT_OBJECT_0) {
        return ConsoleWaitResult::InputReady;
    }
    if (result == WAIT_OBJECT_0 + 1) {
        return ConsoleWaitResult::ShutdownRequested;
    }
    if (result == WAIT_TIMEOUT) {
        return ConsoleWaitResult::Timeout;
    }
    error = result == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE;
    return ConsoleWaitResult::Error;
}

bool ConsoleSession::ShutdownRequested() const noexcept {
    return shutdown_requested.load(std::memory_order_relaxed);
}
