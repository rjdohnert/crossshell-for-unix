#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "env_process.hpp"

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

class InheritedStandardHandles {
public:
    InheritedStandardHandles() = default;
    InheritedStandardHandles(const InheritedStandardHandles&) = delete;
    InheritedStandardHandles& operator=(const InheritedStandardHandles&) = delete;

    ~InheritedStandardHandles() {
        if (attributes_) DeleteProcThreadAttributeList(attributes_);
        for (HANDLE handle : handles_) {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
        }
    }

    bool Initialize(STARTUPINFOEXW& startup, std::wstring& error) {
        const DWORD standardIds[] = {
            STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE
        };
        HANDLE* standardFields[] = {
            &startup.StartupInfo.hStdInput,
            &startup.StartupInfo.hStdOutput,
            &startup.StartupInfo.hStdError
        };
        handles_.reserve(3);

        for (std::size_t index = 0; index < 3; ++index) {
            HANDLE source = GetStdHandle(standardIds[index]);
            if (!source || source == INVALID_HANDLE_VALUE) {
                error = L"a standard process handle is unavailable.";
                return false;
            }
            HANDLE duplicate = nullptr;
            if (!DuplicateHandle(GetCurrentProcess(), source,
                                 GetCurrentProcess(), &duplicate, 0, TRUE,
                                 DUPLICATE_SAME_ACCESS)) {
                error = L"cannot prepare child standard handles (Windows "
                        L"error " + std::to_wstring(GetLastError()) + L").";
                return false;
            }
            handles_.push_back(duplicate);
            *standardFields[index] = duplicate;
        }

        SIZE_T requiredSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &requiredSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            requiredSize == 0) {
            error = L"cannot initialize the child handle list (Windows "
                    L"error " + std::to_wstring(GetLastError()) + L").";
            return false;
        }
        attributeStorage_.resize(requiredSize);
        attributes_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
            attributeStorage_.data());
        if (!InitializeProcThreadAttributeList(attributes_, 1, 0,
                                              &requiredSize)) {
            attributes_ = nullptr;
            error = L"cannot initialize the child handle list (Windows "
                    L"error " + std::to_wstring(GetLastError()) + L").";
            return false;
        }
        if (!UpdateProcThreadAttribute(
                attributes_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                handles_.data(), handles_.size() * sizeof(HANDLE),
                nullptr, nullptr)) {
            error = L"cannot restrict inherited handles (Windows error " +
                std::to_wstring(GetLastError()) + L").";
            return false;
        }
        startup.lpAttributeList = attributes_;
        return true;
    }

private:
    std::vector<HANDLE> handles_;
    std::vector<unsigned char> attributeStorage_;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes_ = nullptr;
};

std::wstring AbsoluteDirectory(const std::wstring& requested,
                               std::wstring& error) {
    std::error_code fileError;
    std::filesystem::path directory = requested.empty()
        ? std::filesystem::current_path(fileError)
        : std::filesystem::path(requested);
    if (fileError) {
        error = L"cannot determine the current directory (Windows error " +
            std::to_wstring(static_cast<unsigned long>(fileError.value())) +
            L").";
        return {};
    }
    if (directory.is_relative()) {
        directory = std::filesystem::absolute(directory, fileError);
        if (fileError) {
            error = L"cannot resolve the working directory.";
            return {};
        }
    }
    if (!std::filesystem::is_directory(directory, fileError) || fileError) {
        error = L"working directory does not exist or is not a directory.";
        return {};
    }
    return directory.lexically_normal().native();
}

bool IsExecutableFile(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool HasPath(const std::wstring& command) {
    return command.find_first_of(L"\\/:") != std::wstring::npos;
}

bool ResolveExecutable(const std::wstring& command,
                       const EnvironmentManager::Map& environment,
                       const std::filesystem::path& workingDirectory,
                       std::wstring& executable) {
    const std::filesystem::path commandPath(command);
    if (commandPath.has_root_name() && !commandPath.has_root_directory()) {
        return false;
    }

    std::vector<std::filesystem::path> directories;
    if (HasPath(command)) {
        directories.push_back(commandPath.has_root_directory()
            ? commandPath.root_path() / commandPath.relative_path()
            : workingDirectory / commandPath);
    } else {
        const auto pathValue = environment.find(L"PATH");
        if (pathValue == environment.end()) return false;
        std::size_t start = 0;
        while (start <= pathValue->second.size()) {
            const std::size_t end = pathValue->second.find(L';', start);
            const std::wstring entry = pathValue->second.substr(
                start, end == std::wstring::npos
                    ? std::wstring::npos : end - start);
            if (!entry.empty()) {
                std::filesystem::path directory(entry);
                if (directory.is_relative()) directory = workingDirectory / directory;
                directories.push_back(directory / commandPath);
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }

    const bool hasExtension = commandPath.has_extension();
    const std::wstring extensions[] = {L".exe", L".com"};
    for (const std::filesystem::path& candidateBase : directories) {
        const std::size_t extensionCount = hasExtension ? 1 : 2;
        for (std::size_t index = 0; index < extensionCount; ++index) {
            std::filesystem::path candidate = candidateBase;
            if (!hasExtension) candidate += extensions[index];
            std::error_code fileError;
            const std::filesystem::path absolute =
                std::filesystem::absolute(candidate, fileError);
            if (fileError || !IsExecutableFile(absolute)) continue;
            executable = absolute.lexically_normal().native();
            return true;
        }
    }
    return false;
}

std::wstring QuoteArgument(std::wstring_view argument) {
    if (argument.empty()) return L"\"\"";
    if (argument.find_first_of(L" \t\n\v\f\r\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }

    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(character);
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring BuildCommandLine(
    const std::vector<std::wstring>& commandAndArgs) {
    std::wstring commandLine;
    for (const std::wstring& argument : commandAndArgs) {
        if (!commandLine.empty()) commandLine.push_back(L' ');
        commandLine += QuoteArgument(argument);
    }
    return commandLine;
}

} // namespace

int ProcessRunner::Execute(
    const std::vector<std::wstring>& commandAndArgs,
    const EnvironmentManager::Map& environment,
    const std::vector<wchar_t>& environmentBlock,
    const std::wstring& workingDirectory,
    bool verbose) {
    if (commandAndArgs.empty()) return 0;

    std::wstring error;
    const std::wstring absoluteDirectory =
        AbsoluteDirectory(workingDirectory, error);
    if (absoluteDirectory.empty()) {
        std::wcerr << L"env: " << error << L'\n';
        return 126;
    }

    std::wstring executable;
    if (!ResolveExecutable(commandAndArgs.front(), environment,
                          absoluteDirectory, executable)) {
        std::wcerr << L"env: '" << commandAndArgs.front()
                   << L"': No such file or directory\n";
        return 127;
    }

    if (verbose) {
        std::wcerr << L"env: executing " << executable << L'\n';
        std::wcerr << L"env: working directory " << absoluteDirectory << L'\n';
    }

    std::wstring commandLine = BuildCommandLine(commandAndArgs);
    std::vector<wchar_t> mutableCommandLine(
        commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    InheritedStandardHandles inheritedHandles;
    if (!inheritedHandles.Initialize(startup, error)) {
        std::wcerr << L"env: " << error << L'\n';
        return 126;
    }

    PROCESS_INFORMATION process{};
    const DWORD creationFlags =
        CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT;
    if (!CreateProcessW(
            executable.c_str(), mutableCommandLine.data(),
            nullptr, nullptr, TRUE, creationFlags,
            const_cast<wchar_t*>(environmentBlock.data()),
            absoluteDirectory.c_str(), &startup.StartupInfo, &process)) {
        const DWORD windowsError = GetLastError();
        std::wcerr << L"env: cannot start '" << executable
                   << L"' (Windows error " << windowsError << L").\n";
        return windowsError == ERROR_FILE_NOT_FOUND ||
                       windowsError == ERROR_PATH_NOT_FOUND
            ? 127 : 126;
    }

    CloseHandle(process.hThread);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD windowsError = GetLastError();
        CloseHandle(process.hProcess);
        std::wcerr << L"env: waiting for the child process failed "
                   << L"(Windows error " << windowsError << L").\n";
        return 126;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) {
        const DWORD windowsError = GetLastError();
        CloseHandle(process.hProcess);
        std::wcerr << L"env: cannot read the child exit status "
                   << L"(Windows error " << windowsError << L").\n";
        return 126;
    }
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
