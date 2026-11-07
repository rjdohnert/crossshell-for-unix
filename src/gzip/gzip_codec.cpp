#define WIN32_LEAN_AND_MEAN
#include "gzip_codec.hpp"
#include "gzip_file.hpp"
#include "gzip_member.hpp"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {
std::wstring SystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length != 0 && buffer != nullptr
        ? std::wstring(buffer, length)
        : L"Windows error " + std::to_wstring(code);
    if (buffer != nullptr) LocalFree(buffer);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

std::wstring QuoteArgument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(character);
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring EscapePowerShellString(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size());
    for (wchar_t character : value) {
        if (character == L'\'') escaped += L"''";
        else escaped.push_back(character);
    }
    return escaped;
}

std::wstring DecodeErrorOutput(const std::string& bytes) {
    if (bytes.empty()) return {};
    const int required = MultiByteToWideChar(GetOEMCP(), 0, bytes.data(),
        static_cast<int>(bytes.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(GetOEMCP(), 0, bytes.data(),
            static_cast<int>(bytes.size()), result.data(), required) <= 0) {
        return {};
    }
    while (!result.empty() &&
           (result.back() == L'\r' || result.back() == L'\n')) {
        result.pop_back();
    }
    return result;
}

bool RunPowerShell(const std::wstring& script, std::wstring& error) {
    std::vector<wchar_t> systemDirectory(32768);
    const UINT systemLength = GetSystemDirectoryW(
        systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
    if (systemLength == 0 || systemLength >= systemDirectory.size()) {
        error = L"cannot locate the Windows system directory: " +
                SystemMessage(GetLastError());
        return false;
    }
    const std::wstring executable =
        std::wstring(systemDirectory.data(), systemLength) +
        L"\\WindowsPowerShell\\v1.0\\powershell.exe";

    std::wstring commandLine = QuoteArgument(executable) +
        L" -NoLogo -NoProfile -NonInteractive -Command " +
        QuoteArgument(script);
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        error = L"cannot create PowerShell diagnostic pipe: " +
                SystemMessage(GetLastError());
        return false;
    }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        error = L"cannot secure PowerShell diagnostic pipe: " +
                SystemMessage(GetLastError());
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        error = L"cannot open null input for PowerShell: " +
                SystemMessage(GetLastError());
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    SIZE_T attributesSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesSize);
    std::vector<unsigned char> attributeStorage(attributesSize);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributesSize)) {
        error = L"cannot initialize PowerShell process attributes: " +
                SystemMessage(GetLastError());
        CloseHandle(nullInput);
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    HANDLE inheritedHandles[] = {writePipe, nullInput};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr)) {
        error = L"cannot restrict PowerShell inherited handles: " +
                SystemMessage(GetLastError());
        DeleteProcThreadAttributeList(attributes);
        CloseHandle(nullInput);
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullInput;
    startup.StartupInfo.hStdOutput = writePipe;
    startup.StartupInfo.hStdError = writePipe;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(executable.c_str(), mutableCommand.data(),
        nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr,
        &startup.StartupInfo, &process);

    CloseHandle(nullInput);
    CloseHandle(writePipe);
    DeleteProcThreadAttributeList(attributes);
    if (!created) {
        error = L"cannot start Windows PowerShell: " + SystemMessage(GetLastError());
        CloseHandle(readPipe);
        return false;
    }

    std::string output;
    std::vector<char> buffer(4096);
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(readPipe, buffer.data(),
                      static_cast<DWORD>(buffer.size()), &bytesRead, nullptr)) {
            const DWORD code = GetLastError();
            if (code != ERROR_BROKEN_PIPE) {
                error = L"cannot read PowerShell diagnostics: " + SystemMessage(code);
                break;
            }
            break;
        }
        if (bytesRead == 0) break;
        const size_t remaining = output.size() < 65536 ? 65536 - output.size() : 0;
        output.append(buffer.data(), (std::min)(remaining,
                                              static_cast<size_t>(bytesRead)));
    }
    CloseHandle(readPipe);

    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    const bool gotExitCode = waitResult == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!gotExitCode) {
        if (error.empty()) {
            error = L"cannot collect PowerShell exit status: " +
                    SystemMessage(GetLastError());
        }
        return false;
    }
    if (exitCode != 0) {
        const std::wstring details = DecodeErrorOutput(output);
        error = details.empty()
            ? L"PowerShell GZipStream failed with exit code " +
                  std::to_wstring(exitCode)
            : details;
        return false;
    }
    return error.empty();
}

std::wstring BuildCodecScript(const std::filesystem::path& source,
                              const std::filesystem::path& output,
                              bool compress,
                              const std::filesystem::path& membersManifest) {
    const std::wstring sourceLiteral =
        L"'" + EscapePowerShellString(source.native()) + L"'";
    const std::wstring outputLiteral =
        L"'" + EscapePowerShellString(output.native()) + L"'";
    const std::wstring manifestLiteral =
        L"'" + EscapePowerShellString(membersManifest.native()) + L"'";
    std::wstring script =
        L"$ErrorActionPreference='Stop'; $failed=$false; "
        L"$inputStream=$null; $outputStream=$null; $codec=$null; $bounded=$null; "
        L"try { "
        L"$inputStream=[System.IO.File]::OpenRead(" + sourceLiteral + L"); ";

    script +=
        L"$outputStream=[System.IO.File]::Open(" + outputLiteral +
        L",[System.IO.FileMode]::Open,[System.IO.FileAccess]::Write,"
        L"[System.IO.FileShare]::ReadWrite); $outputStream.SetLength(0); ";
    if (compress) {
        script +=
            L"$codec=[System.IO.Compression.GZipStream]::new($outputStream,"
            L"[System.IO.Compression.CompressionMode]::Compress,$true); "
            L"$inputStream.CopyTo($codec); ";
    } else {
        script +=
            L"$typeDefinition='using System; using System.IO; "
            L"public sealed class GzipBoundedReadStream : Stream { "
            L"private readonly Stream inner; private long remaining; "
            L"public GzipBoundedReadStream(Stream stream,long length) { inner=stream; remaining=length; } "
            L"public long Remaining { get { return remaining; } } "
            L"public override int Read(byte[] buffer,int offset,int count) { "
            L"if (remaining<=0) return 0; int read=inner.Read(buffer,offset,(int)Math.Min((long)count,remaining)); "
            L"remaining-=read; return read; } "
            L"public override int ReadByte() { if (remaining<=0) return -1; int value=inner.ReadByte(); "
            L"if (value>=0) remaining--; return value; } "
            L"public override bool CanRead { get { return true; } } "
            L"public override bool CanSeek { get { return false; } } "
            L"public override bool CanWrite { get { return false; } } "
            L"public override long Length { get { throw new NotSupportedException(); } } "
            L"public override long Position { get { throw new NotSupportedException(); } "
            L"set { throw new NotSupportedException(); } } "
            L"public override void Flush() {} "
            L"public override long Seek(long offset,SeekOrigin origin) { throw new NotSupportedException(); } "
            L"public override void SetLength(long value) { throw new NotSupportedException(); } "
            L"public override void Write(byte[] buffer,int offset,int count) { throw new NotSupportedException(); } "
            L"protected override void Dispose(bool disposing) { base.Dispose(disposing); } "
            L"}'; Add-Type -TypeDefinition $typeDefinition; "
            L"$members=[System.IO.File]::ReadAllLines(" + manifestLiteral + L"); "
            L"foreach ($member in $members) { "
            L"$parts=$member.Split([char]','); if ($parts.Length -ne 2) { throw 'invalid gzip member manifest' }; "
            L"$offset=[Int64]::Parse($parts[0],[Globalization.CultureInfo]::InvariantCulture); "
            L"$length=[Int64]::Parse($parts[1],[Globalization.CultureInfo]::InvariantCulture); "
            L"$inputStream.Position=$offset; $bounded=[GzipBoundedReadStream]::new($inputStream,$length); "
            L"$codec=[System.IO.Compression.GZipStream]::new($bounded,"
            L"[System.IO.Compression.CompressionMode]::Decompress,$true); "
            L"$codec.CopyTo($outputStream); $codec.Dispose(); $codec=$null; "
            L"if ($bounded.Remaining -ne 0) { throw 'gzip member was not fully consumed' }; "
            L"$bounded.Dispose(); $bounded=$null; "
            L"} ";
    }
    script +=
        L"if ($codec) { $codec.Dispose(); $codec=$null }; $outputStream.Flush($true); "
        L"$outputStream.Dispose(); $outputStream=$null; "
        L"$inputStream.Dispose(); $inputStream=$null; "
        L"} catch { [Console]::Error.WriteLine($_.Exception.Message); $failed=$true } "
        L"finally { "
        L"if ($codec) { try { $codec.Dispose() } catch { $failed=$true } }; "
        L"if ($bounded) { try { $bounded.Dispose() } catch { $failed=$true } }; "
        L"if ($outputStream) { try { $outputStream.Dispose() } catch { $failed=$true } }; "
        L"if ($inputStream) { try { $inputStream.Dispose() } catch { $failed=$true } } "
        L"}; if ($failed) { exit 1 }; exit 0";
    return script;
}
}

bool GzipCodec::run(const std::filesystem::path& source,
                    const std::filesystem::path& temporaryOutput,
                    bool compress, std::wstring& error) {
    error.clear();
    GzipTempFile manifest;
    if (!compress) {
        std::vector<GzipMember> members;
        if (!FindGzipMembers(source, members, error)) return false;
        if (!manifest.createInSystemTemp(error)) return false;
        for (const GzipMember& member : members) {
            const std::string entry = std::to_string(member.offset) + "," +
                std::to_string(member.length) + "\n";
            if (!manifest.write(entry.data(), entry.size(), error)) return false;
        }
        if (!manifest.finish(error)) return false;
    }

    return RunPowerShell(BuildCodecScript(source, temporaryOutput, compress,
                                           compress ? std::filesystem::path()
                                                    : manifest.path()),
                         error);
}
