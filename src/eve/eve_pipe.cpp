#define NOMINMAX
#include <windows.h>

#include "eve_pipe.h"

#include <cstddef>
#include <string>
#include <vector>

namespace {

constexpr DWORD kPipeTimeoutMs = 120000;

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    HANDLE get() const noexcept { return value_; }
    HANDLE release() noexcept {
        HANDLE value = value_;
        value_ = nullptr;
        return value;
    }
    void reset(HANDLE value = nullptr) noexcept {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
        value_ = value;
    }

private:
    HANDLE value_;
};

void SetError(std::string& error, const char* action, DWORD code) {
    error = action;
    error += " (Windows error ";
    error += std::to_string(code);
    error += ")";
}

bool DuplicateInheritable(HANDLE source, UniqueHandle& duplicate, std::string& error) {
    if (source == nullptr || source == INVALID_HANDLE_VALUE) {
        error = "standard output or error is not available";
        return false;
    }
    HANDLE value = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &value,
        0, TRUE, DUPLICATE_SAME_ACCESS)) {
        SetError(error, "unable to duplicate a standard handle", GetLastError());
        return false;
    }
    duplicate.reset(value);
    return true;
}

bool StopChild(HANDLE process) noexcept {
    if (!TerminateProcess(process, 1) && GetLastError() != ERROR_ACCESS_DENIED) {
        return false;
    }
    return WaitForSingleObject(process, 5000) == WAIT_OBJECT_0;
}

}

bool WriteCommandInput(const std::string& command_line, const std::string& input_data, std::string& error) {
    if (command_line.empty() || command_line.size() >= 32767 ||
        command_line.find('\0') != std::string::npos) {
        error = "pipe command is empty or exceeds the Windows command-line limit";
        return false;
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE child_input_raw = nullptr;
    HANDLE parent_input_raw = nullptr;
    if (!CreatePipe(&child_input_raw, &parent_input_raw, &security, 0)) {
        SetError(error, "unable to create pipe", GetLastError());
        return false;
    }
    UniqueHandle child_input(child_input_raw);
    UniqueHandle parent_input(parent_input_raw);
    if (!SetHandleInformation(parent_input.get(), HANDLE_FLAG_INHERIT, 0)) {
        SetError(error, "unable to secure parent pipe handle", GetLastError());
        return false;
    }

    UniqueHandle child_output;
    UniqueHandle child_error;
    if (!DuplicateInheritable(GetStdHandle(STD_OUTPUT_HANDLE), child_output, error) ||
        !DuplicateInheritable(GetStdHandle(STD_ERROR_HANDLE), child_error, error)) {
        return false;
    }

    SIZE_T attributes_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes_size);
    if (attributes_size == 0) {
        SetError(error, "unable to size process handle list", GetLastError());
        return false;
    }
    std::vector<std::byte> attribute_storage(attributes_size);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributes_size)) {
        SetError(error, "unable to initialize process handle list", GetLastError());
        return false;
    }

    HANDLE inherited_handles[] = {
        child_input.get(), child_output.get(), child_error.get()
    };
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited_handles, sizeof(inherited_handles), nullptr, nullptr)) {
        const DWORD code = GetLastError();
        DeleteProcThreadAttributeList(attributes);
        SetError(error, "unable to restrict child handle inheritance", code);
        return false;
    }

    STARTUPINFOEXA startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = child_input.get();
    startup.StartupInfo.hStdOutput = child_output.get();
    startup.StartupInfo.hStdError = child_error.get();
    startup.lpAttributeList = attributes;

    PROCESS_INFORMATION process_info{};
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');
    const BOOL created = CreateProcessA(
        nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process_info);
    const DWORD create_error = created ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(attributes);
    child_input.reset();
    child_output.reset();
    child_error.reset();

    if (!created) {
        SetError(error, "unable to start pipe command", create_error);
        return false;
    }
    UniqueHandle process(process_info.hProcess);
    UniqueHandle thread(process_info.hThread);

    size_t offset = 0;
    while (offset < input_data.size()) {
        const size_t remaining = input_data.size() - offset;
        const DWORD chunk_size = remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
        DWORD written = 0;
        const BOOL write_succeeded = WriteFile(
            parent_input.get(), input_data.data() + offset, chunk_size, &written, nullptr);
        if (!write_succeeded || written == 0) {
            const DWORD code = write_succeeded ? ERROR_WRITE_FAULT : GetLastError();
            parent_input.reset();
            SetError(error, "unable to write all data to pipe command", code);
            if (!StopChild(process.get())) error += "; child process could not be stopped";
            return false;
        }
        offset += written;
    }
    parent_input.reset();

    const DWORD wait_result = WaitForSingleObject(process.get(), kPipeTimeoutMs);
    if (wait_result == WAIT_TIMEOUT) {
        error = "pipe command exceeded the 120-second timeout";
        if (!StopChild(process.get())) error += "; child process could not be stopped";
        return false;
    }
    if (wait_result != WAIT_OBJECT_0) {
        const DWORD code = GetLastError();
        SetError(error, "unable to wait for pipe command", code);
        if (!StopChild(process.get())) error += "; child process could not be stopped";
        return false;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process.get(), &exit_code)) {
        SetError(error, "unable to read pipe command exit status", GetLastError());
        return false;
    }
    if (exit_code != 0) {
        error = "pipe command exited with status ";
        error += std::to_string(exit_code);
        return false;
    }
    return true;
}
