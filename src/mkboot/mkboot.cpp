/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <imapi2.h>
#include <imapi2fs.h>
#include <wrl/client.h>
#include <comdef.h>

#if __has_include(<DismApi.h>) || __has_include(<dismapi.h>)
#define MKBOOT_HAS_DISM 1
#if __has_include(<DismApi.h>)
#include <DismApi.h>
#else
#include <dismapi.h>
#endif
#else
#define MKBOOT_HAS_DISM 0
#endif

#include <iostream>
#include <string>
#include <vector>
#ifndef _HAS_CXX17
#define _HAS_CXX17 1
#endif
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#if MKBOOT_HAS_DISM
#pragma comment(lib, "dismapi.lib")
#endif
#pragma comment(lib, "comsuppw.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

// Signal flag for asynchronous process interruption
static std::atomic<bool> g_AbortRequested{ false };

// ============================================================================
// CONSOLE CONTROL HANDLER (LOCK-FREE ASYNCHRONOUS SIGNALING)
// ============================================================================

BOOL WINAPI ConsoleControlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        g_AbortRequested.store(true);
        return TRUE; // Handled. Main thread loop will detect flag and unwind safely.
    }
    return FALSE;
}

// ============================================================================
// LOGGING & SYSTEM UTILITIES
// ============================================================================

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void SetVerbose(bool enable) { s_Verbose = enable; }

    static void Log(LogLevel level, const std::string& message) {
        if (level == LogLevel::Debug && !s_Verbose) return;

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        WORD originalAttrs = consoleInfo.wAttributes;

        switch (level) {
        case LogLevel::Info:
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            std::cout << "[INFO] ";
            break;
        case LogLevel::Warning:
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            std::cout << "[WARN] ";
            break;
        case LogLevel::Error:
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::cerr << "[ERROR] ";
            break;
        case LogLevel::Debug:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY);
            std::cout << "[DEBUG] ";
            break;
        }

        SetConsoleTextAttribute(hConsole, originalAttrs);
        if (level == LogLevel::Error) {
            std::cerr << message << std::endl;
        } else {
            std::cout << message << std::endl;
        }
    }

private:
    static bool s_Verbose;
};

bool Logger::s_Verbose = false;

std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string GetHResultErrorMessage(HRESULT hr) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    if (message.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase << hr;
        return ss.str();
    }
    return message;
}

// Native NT Token Information Elevation Verification
bool IsElevated() {
    bool elevated = false;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            elevated = (elevation.TokenIsElevated != 0);
        }
        CloseHandle(hToken);
    }
    return elevated;
}

// Pre-flight Disk Space Validation via GetDiskFreeSpaceExW
bool HasSufficientDiskSpace(const fs::path& targetPath, ULONGLONG requiredBytes) {
    fs::path checkPath = targetPath;
    if (!fs::exists(checkPath)) {
        checkPath = checkPath.parent_path();
    }

    ULARGE_INTEGER freeBytesAvailableToCaller = { 0 };
    ULARGE_INTEGER totalNumberOfBytes = { 0 };
    ULARGE_INTEGER totalNumberOfFreeBytes = { 0 };

    if (GetDiskFreeSpaceExW(checkPath.wstring().c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        return freeBytesAvailableToCaller.QuadPart >= requiredBytes;
    }
    return true; // Fallback if query unsupported
}

// ============================================================================
// RAII RESOURCE GUARDS
// ============================================================================

class ScopedCOM {
public:
    ScopedCOM() {
        m_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }
    ~ScopedCOM() {
        if (SUCCEEDED(m_hr)) {
            CoUninitialize();
        }
    }
    bool IsSucceeded() const { return SUCCEEDED(m_hr); }
    HRESULT GetResult() const { return m_hr; }
private:
    HRESULT m_hr;
};

#if MKBOOT_HAS_DISM
class ScopedDism {
public:
    ScopedDism() {
        m_hr = DismInitialize(DismLogErrorsWarningsInfo, L"mkboot_dism.log", NULL);
    }
    ~ScopedDism() {
        if (SUCCEEDED(m_hr)) {
            DismShutdown();
        }
    }
    bool IsSucceeded() const { return SUCCEEDED(m_hr); }
    HRESULT GetResult() const { return m_hr; }
private:
    HRESULT m_hr;
};
#else
class ScopedDism {
public:
    ScopedDism() : m_hr(E_FAIL) {}
    bool IsSucceeded() const { return false; }
    HRESULT GetResult() const { return m_hr; }
private:
    HRESULT m_hr;
};
#endif

class ScopedTempDir {
public:
    ScopedTempDir() {
        wchar_t tempPathBuffer[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPathBuffer);
        wchar_t uniquePath[MAX_PATH];
        GetTempFileNameW(tempPathBuffer, L"MKB", 0, uniquePath);

        m_Path = uniquePath;
        fs::remove(m_Path);
        fs::create_directories(m_Path);
    }

    ~ScopedTempDir() {
        try {
            if (fs::exists(m_Path)) {
                fs::remove_all(m_Path);
            }
        } catch (...) {
            // Suppress cleanup exceptions during exit
        }
    }

    const fs::path& GetPath() const { return m_Path; }

private:
    fs::path m_Path;
};

// ============================================================================
// CONFIGURATION & CLI PARSER
// ============================================================================

struct ConfigOptions {
    fs::path sourcePath;
    fs::path outputIsoPath;
    fs::path bootFilePath;
    fs::path mountPath;
    std::vector<fs::path> driverPaths;
    std::vector<std::string> execCommands;
    std::string architecture = "x64";
    std::string volumeLabel = "WINPE_BOOT";
    bool ignoreDriverErrors = false;
    bool force = false;
    bool verbose = false;
    bool showHelp = false;
};

void PrintHelp(const char* exeName) {
    std::cout << R"(
===============================================================================
  mkboot v16.7.01 (WinPE Engine & ISO Builder)
===============================================================================

DESCRIPTION:
    Creates and configures bootable WinPE image files and bootable ISO images.
    This utility is inspired by HP-UX mkboot CLI semantics while applying native 
    Windows DISM servicing and IMAPI2 El Torito bootable filesystem structures.

USAGE:
    )" << exeName << R"( -s <SourceDir|WimFile> -o <Output.iso> [OPTIONS]

REQUIRED PARAMETERS:
    -s, --source <path>         Path to WinPE source tree or base boot.wim file.
    -o, --output <path>         Path for the generated bootable .iso image file.

OPTIONS:
    -b, --boot-file <path>      Custom El Torito boot file (e.g., etfsboot.com or
                                efisys.bin). Default auto-detects from source.
    -d, --driver <path>         Inject driver (.inf file or folder) into WinPE.
                                Can be specified multiple times.
    --ignore-driver-errors      Proceed with commit even if a driver fails injection.
    -e, --exec <command>        Inject startup auto-execute command into WinPE 
                                (equivalent to HP-UX mkboot -a parameter).
                                Appends execution string to startnet.cmd.
    -a, --arch <arch>           Target Architecture: x64, x86, or arm64. Default: x64
    -l, --label <label>         Volume label for created ISO (default: WINPE_BOOT).
    -m, --mount-dir <path>      Explicit directory to mount WIM during modification.
    -f, --force                 Force overwriting existing destination files.
    -v, --verbose               Enable verbose output and detailed DISM/IMAPI logging.
    -h, --help                  Display this comprehensive help manual.

EXAMPLES:
    1. Create basic bootable WinPE ISO from folder source:
       mkboot -s C:\WinPE_amd64\media -o C:\Images\bootable.iso

    2. Service WIM with drivers and startup commands (HP-UX -a equivalent):
       mkboot -s C:\WinPE_amd64\media -o C:\Images\custom_boot.iso \
              -d C:\Drivers\Network -e "netuse Z: \\server\share /user:admin pass" \
              -e "Z:\setup.exe"

    3. Service WIM and ignore non-critical driver injection warnings:
       mkboot -s C:\WinPE_amd64\media -o C:\Images\custom_boot.iso \
              -d C:\Drivers\Experimental --ignore-driver-errors

    4. Overwrite output and specify explicit bootloader:
       mkboot -s C:\WinPE_amd64 -o C:\boot.iso -b C:\WinPE_amd64\Boot\etfsboot.com --force
)" << std::endl;
}

bool ParseCommandLine(int argc, char* argv[], ConfigOptions& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            config.showHelp = true;
            return true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
            Logger::SetVerbose(true);
        } else if (arg == "-f" || arg == "--force") {
            config.force = true;
        } else if (arg == "--ignore-driver-errors") {
            config.ignoreDriverErrors = true;
        } else if (arg == "-s" || arg == "--source") {
            if (i + 1 < argc) config.sourcePath = argv[++i];
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) config.outputIsoPath = argv[++i];
        } else if (arg == "-b" || arg == "--boot-file") {
            if (i + 1 < argc) config.bootFilePath = argv[++i];
        } else if (arg == "-d" || arg == "--driver") {
            if (i + 1 < argc) config.driverPaths.emplace_back(argv[++i]);
        } else if (arg == "-e" || arg == "--exec" || arg == "-a" || arg == "--auto-exec") {
            if (i + 1 < argc) config.execCommands.emplace_back(argv[++i]);
        } else if (arg == "-l" || arg == "--label") {
            if (i + 1 < argc) config.volumeLabel = argv[++i];
        } else if (arg == "-m" || arg == "--mount-dir") {
            if (i + 1 < argc) config.mountPath = argv[++i];
        } else if (arg == "--arch") {
            if (i + 1 < argc) config.architecture = argv[++i];
        } else {
            Logger::Log(LogLevel::Error, "Unknown command line option: " + arg);
            return false;
        }
    }
    return true;
}

// ============================================================================
// DISM WINPE SERVICING ENGINE
// ============================================================================

class WinPEManager {
public:
    static bool CustomizeImage(const fs::path& wimPath, const fs::path& mountDir, const ConfigOptions& config) {
#if !MKBOOT_HAS_DISM
        (void)wimPath;
        (void)mountDir;
        (void)config;
        Logger::Log(LogLevel::Error, "DISM support is unavailable because the Windows ADK/Deployment Tools headers are not installed.");
        return false;
#else
        if (g_AbortRequested.load()) return false;

        Logger::Log(LogLevel::Info, "Mounting WinPE image: " + wimPath.string());

        std::wstring wWimPath = wimPath.wstring();
        std::wstring wMountDir = mountDir.wstring();

        HRESULT hr = DismMountImage(
            wWimPath.c_str(),
            wMountDir.c_str(),
            1,
            NULL,
            DismImageIndex,
            DismReadWrite,
            NULL, NULL, NULL
        );

        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "DismMountImage failed: " + GetHResultErrorMessage(hr));
            return false;
        }

        Logger::Log(LogLevel::Info, "Image mounted successfully to " + mountDir.string());

        bool success = true;
        DismSession session = 0;

        hr = DismOpenSession(wMountDir.c_str(), NULL, NULL, &session);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "DismOpenSession failed: " + GetHResultErrorMessage(hr));
            DismUnmountImage(wMountDir.c_str(), 0, NULL, NULL, NULL);
            return false;
        }

        // Inject Driver Packages
        for (const auto& driverPath : config.driverPaths) {
            if (g_AbortRequested.load()) {
                Logger::Log(LogLevel::Warning, "Process interrupt detected. Aborting driver servicing...");
                success = false;
                break;
            }

            Logger::Log(LogLevel::Info, "Injecting driver(s) from: " + driverPath.string());
            std::wstring wDriverPath = driverPath.wstring();

            hr = DismAddDriver(session, wDriverPath.c_str(), FALSE);
            if (FAILED(hr)) {
                Logger::Log(LogLevel::Warning, "Failed driver injection (" + driverPath.string() + "): " + GetHResultErrorMessage(hr));
                if (!config.ignoreDriverErrors) {
                    Logger::Log(LogLevel::Error, "Aborting WIM servicing due to driver error. Pass --ignore-driver-errors to bypass.");
                    success = false;
                    break;
                }
            } else {
                Logger::Log(LogLevel::Info, "Driver successfully injected.");
            }
        }

        // Inject Startup Commands (-a / -e parity)
        if (success && !config.execCommands.empty() && !g_AbortRequested.load()) {
            fs::path startnetPath = mountDir / "Windows" / "System32" / "startnet.cmd";
            Logger::Log(LogLevel::Info, "Injecting auto-execution lines into " + startnetPath.string());

            if (fs::exists(startnetPath)) {
                std::ofstream file(startnetPath, std::ios::app);
                if (file.is_open()) {
                    file << "\r\n:: --- Injected by mkboot Windows Utility ---\r\n";
                    for (const auto& cmd : config.execCommands) {
                        file << cmd << "\r\n";
                        Logger::Log(LogLevel::Debug, "Appended: " + cmd);
                    }
                    file.close();
                } else {
                    Logger::Log(LogLevel::Error, "Failed to open startnet.cmd for writing.");
                    success = false;
                }
            } else {
                Logger::Log(LogLevel::Error, "startnet.cmd not found in mounted WIM image.");
                success = false;
            }
        }

        // Check for process cancellation before unmount commit
        if (g_AbortRequested.load()) {
            Logger::Log(LogLevel::Warning, "Abort requested prior to WIM commit. Discarding changes...");
            success = false;
        }

        // Commit or discard the WIM session, then unmount.
        if (success) {
            Logger::Log(LogLevel::Info, "Committing WIM changes...");
            hr = DismCommitImage(session, 0, NULL, NULL, NULL);
            if (FAILED(hr)) {
                Logger::Log(LogLevel::Error, "DismCommitImage failed: " + GetHResultErrorMessage(hr));
                DismCloseSession(session);
                DismUnmountImage(wMountDir.c_str(), 0, NULL, NULL, NULL);
                return false;
            }
        } else {
            Logger::Log(LogLevel::Info, "Discarding WIM changes...");
        }

        DismCloseSession(session);

        Logger::Log(LogLevel::Info, std::string("Unmounting image (") + (success ? "after commit)..." : "after discard)..."));
        hr = DismUnmountImage(wMountDir.c_str(), 0, NULL, NULL, NULL);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "DismUnmountImage failed: " + GetHResultErrorMessage(hr));
            return false;
        }

        if (success) {
            Logger::Log(LogLevel::Info, "WinPE WIM servicing finished successfully.");
        }
        return success;
#endif
    }
};

// ============================================================================
// HARDENED IMAPI2 ISO BUILDER
// ============================================================================

class IsoBuilder {
public:
    static bool BuildIso(const fs::path& sourceTree, const fs::path& bootFileOverride, const fs::path& outputIso, std::string volumeLabel) {
        if (g_AbortRequested.load()) return false;

        Logger::Log(LogLevel::Info, "Initializing IMAPI2 Engine...");

        if (volumeLabel.length() > 32) {
            volumeLabel = volumeLabel.substr(0, 32);
            Logger::Log(LogLevel::Warning, "Volume label truncated to 32 chars: " + volumeLabel);
        }

        ComPtr<IFileSystemImage> pFileSystemImage;
        HRESULT hr = CoCreateInstance(
            CLSID_MsftFileSystemImage,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pFileSystemImage)
        );

        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "Failed to initialize IFileSystemImage: " + GetHResultErrorMessage(hr));
            return false;
        }

        pFileSystemImage->put_FileSystemsToCreate(
            (FsiFileSystems)(FsiFileSystemISO9660 | FsiFileSystemJoliet | FsiFileSystemUDF)
        );

        pFileSystemImage->put_VolumeName(_bstr_t(volumeLabel.c_str()));

        // Resolve BIOS / UEFI El Torito Boot Options
        fs::path biosBootFile = bootFileOverride;
        fs::path efiBootFile = sourceTree / "efi" / "microsoft" / "boot" / "efisys.bin";

        if (biosBootFile.empty()) {
            fs::path potentialEtfs = sourceTree / "boot" / "etfsboot.com";
            if (fs::exists(potentialEtfs)) {
                biosBootFile = potentialEtfs;
            }
        }

        ComPtr<IBootOptions> pBiosBootOptions;
        ComPtr<IBootOptions> pEfiBootOptions;

        if (!biosBootFile.empty() && fs::exists(biosBootFile)) {
            Logger::Log(LogLevel::Info, "Configuring El Torito BIOS boot entry: " + biosBootFile.string());
            hr = CoCreateInstance(CLSID_BootOptions, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pBiosBootOptions));
            if (SUCCEEDED(hr)) {
                ComPtr<IStream> pBootStream;
                hr = SHCreateStreamOnFileW(biosBootFile.wstring().c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &pBootStream);
                if (SUCCEEDED(hr)) {
                    pBiosBootOptions->AssignBootImage(pBootStream.Get());
                    pBiosBootOptions->put_Manufacturer(_bstr_t(L"Microsoft"));
                    pBiosBootOptions->put_PlatformId(PlatformX86);
                    pBiosBootOptions->put_Emulation(EmulationNone);
                }
            }
        }

        if (fs::exists(efiBootFile)) {
            Logger::Log(LogLevel::Info, "Configuring El Torito UEFI boot entry: " + efiBootFile.string());
            hr = CoCreateInstance(CLSID_BootOptions, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEfiBootOptions));
            if (SUCCEEDED(hr)) {
                ComPtr<IStream> pBootStream;
                hr = SHCreateStreamOnFileW(efiBootFile.wstring().c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &pBootStream);
                if (SUCCEEDED(hr)) {
                    pEfiBootOptions->AssignBootImage(pBootStream.Get());
                    pEfiBootOptions->put_Manufacturer(_bstr_t(L"Microsoft"));
                    pEfiBootOptions->put_PlatformId(PlatformEFI);
                    pEfiBootOptions->put_Emulation(EmulationNone);
                }
            }
        }

        // Attach Multi-Boot SAFEARRAY if both platforms are available
        ComPtr<IFileSystemImage2> pFileSystemImage2;
        if (pBiosBootOptions && pEfiBootOptions && SUCCEEDED(pFileSystemImage.As(&pFileSystemImage2))) {
            Logger::Log(LogLevel::Info, "Binding Dual BIOS + UEFI boot option array...");
            SAFEARRAY* pSa = SafeArrayCreateVector(VT_DISPATCH, 0, 2);
            LONG idx0 = 0, idx1 = 1;
            SafeArrayPutElement(pSa, &idx0, pBiosBootOptions.Get());
            SafeArrayPutElement(pSa, &idx1, pEfiBootOptions.Get());
            hr = pFileSystemImage2->put_BootImageOptionsArray(pSa);
            SafeArrayDestroy(pSa);
        } else if (pBiosBootOptions) {
            pFileSystemImage->put_BootImageOptions(pBiosBootOptions.Get());
        } else if (pEfiBootOptions) {
            pFileSystemImage->put_BootImageOptions(pEfiBootOptions.Get());
        }

        // Import Root File System Tree
        Logger::Log(LogLevel::Info, "Importing file structure into ISO stream...");
        ComPtr<IFsiDirectoryItem> pRootDir;
        hr = pFileSystemImage->get_Root(&pRootDir);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "Failed to acquire ISO root directory: " + GetHResultErrorMessage(hr));
            return false;
        }

        hr = pRootDir->AddTree(_bstr_t(sourceTree.string().c_str()), VARIANT_FALSE);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "Failed importing tree into ISO: " + GetHResultErrorMessage(hr));
            return false;
        }

        // Generate Resulting ISO Stream
        Logger::Log(LogLevel::Info, "Compiling image stream...");
        ComPtr<IFileSystemImageResult> pResult;
        hr = pFileSystemImage->CreateResultImage(&pResult);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "CreateResultImage failed: " + GetHResultErrorMessage(hr));
            return false;
        }

        ComPtr<IStream> pImageStream;
        hr = pResult->get_ImageStream(&pImageStream);
        if (FAILED(hr)) {
            Logger::Log(LogLevel::Error, "Failed acquiring output stream: " + GetHResultErrorMessage(hr));
            return false;
        }

        // Stream ISO directly to disk with cancellation checks
        Logger::Log(LogLevel::Info, "Writing ISO image to disk: " + outputIso.string());
        return WriteStreamToFile(pImageStream.Get(), outputIso);
    }

private:
    static bool WriteStreamToFile(IStream* pStream, const fs::path& outputPath) {
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            Logger::Log(LogLevel::Error, "Failed opening output ISO file stream.");
            return false;
        }

        const ULONG bufferSize = 64 * 1024; // 64 KB chunk buffer
        std::vector<BYTE> buffer(bufferSize);
        ULONG bytesRead = 0;

        do {
            if (g_AbortRequested.load()) {
                Logger::Log(LogLevel::Error, "ISO creation aborted by signal interrupt.");
                outFile.close();
                fs::remove(outputPath);
                return false;
            }

            HRESULT hr = pStream->Read(buffer.data(), bufferSize, &bytesRead);
            if (FAILED(hr)) {
                Logger::Log(LogLevel::Error, "Failed reading ISO image block: " + GetHResultErrorMessage(hr));
                outFile.close();
                return false;
            }

            if (bytesRead > 0) {
                outFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
                if (!outFile.good()) {
                    Logger::Log(LogLevel::Error, "Disk write error occurred while emitting ISO stream.");
                    outFile.close();
                    return false;
                }
            }
        } while (bytesRead > 0);

        outFile.close();
        return true;
    }
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    // Register Lock-Free Control Signal Handler
    SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);

    ConfigOptions config;
    if (!ParseCommandLine(argc, argv, config)) {
        return 1;
    }

    if (config.showHelp || argc == 1) {
        PrintHelp(argv[0]);
        return 0;
    }

    // Input Validation
    if (config.sourcePath.empty()) {
        Logger::Log(LogLevel::Error, "Source path (-s, --source) is required.");
        return 1;
    }

    if (config.outputIsoPath.empty()) {
        Logger::Log(LogLevel::Error, "Output ISO path (-o, --output) is required.");
        return 1;
    }

    // Path Normalization to Absolute Win32 Paths
    try {
        config.sourcePath = fs::absolute(config.sourcePath);
        config.outputIsoPath = fs::absolute(config.outputIsoPath);
        if (!config.bootFilePath.empty()) config.bootFilePath = fs::absolute(config.bootFilePath);
        if (!config.mountPath.empty()) config.mountPath = fs::absolute(config.mountPath);
        for (auto& dp : config.driverPaths) dp = fs::absolute(dp);
    } catch (const std::exception& ex) {
        Logger::Log(LogLevel::Error, std::string("Path normalization error: ") + ex.what());
        return 1;
    }

    if (!fs::exists(config.sourcePath)) {
        Logger::Log(LogLevel::Error, "Source directory or WIM file does not exist: " + config.sourcePath.string());
        return 1;
    }

    if (fs::exists(config.outputIsoPath) && !config.force) {
        Logger::Log(LogLevel::Error, "Output file already exists. Specify -f or --force to overwrite.");
        return 1;
    }

    // Verify Administrative Elevation using NT Token Information
    bool needsDism = (!config.driverPaths.empty() || !config.execCommands.empty());
#if !MKBOOT_HAS_DISM
    if (needsDism) {
        Logger::Log(LogLevel::Error, "DISM support is unavailable because the Windows ADK/Deployment Tools headers are not installed.");
        Logger::Log(LogLevel::Error, "Install the Windows ADK and add its include path to continue with WinPE servicing.");
        return 1;
    }
#else
    if (needsDism && !IsElevated()) {
        Logger::Log(LogLevel::Error, "Administrative elevation is required for DISM driver/script servicing.");
        Logger::Log(LogLevel::Error, "Please rerun this command from an elevated Administrator console.");
        return 1;
    }
#endif

    // Pre-flight Disk Space Validation
    ULONGLONG estimatedBytesNeeded = 500 * 1024 * 1024; // 500 MB baseline
    if (fs::is_regular_file(config.sourcePath)) {
        estimatedBytesNeeded = fs::file_size(config.sourcePath) * 3; // WIM + Extracted Mount + Output ISO
    }

    if (!HasSufficientDiskSpace(config.outputIsoPath, estimatedBytesNeeded)) {
        Logger::Log(LogLevel::Error, "Insufficient free disk space available for ISO compilation.");
        return 1;
    }

    // Initialize System COM Infrastructure
    ScopedCOM comGuard;
    if (!comGuard.IsSucceeded()) {
        Logger::Log(LogLevel::Error, "Failed initializing COM subsystem: " + GetHResultErrorMessage(comGuard.GetResult()));
        return 1;
    }

    // Initialize System DISM Framework
    ScopedDism dismGuard;
    if (!dismGuard.IsSucceeded()) {
        Logger::Log(LogLevel::Error, "Failed initializing DISM framework: " + GetHResultErrorMessage(dismGuard.GetResult()));
        return 1;
    }

    // Setup Temp Workspace
    ScopedTempDir workingDir;
    fs::path sourceTreePath;
    fs::path targetWimPath;

    if (fs::is_regular_file(config.sourcePath) && config.sourcePath.extension() == ".wim") {
        sourceTreePath = workingDir.GetPath() / "iso_root";
        fs::create_directories(sourceTreePath / "sources");
        targetWimPath = sourceTreePath / "sources" / "boot.wim";

        Logger::Log(LogLevel::Info, "Copying base WIM file into temporary build root...");
        fs::copy_file(config.sourcePath, targetWimPath, fs::copy_options::overwrite_existing);
    } else if (fs::is_directory(config.sourcePath)) {
        sourceTreePath = config.sourcePath;
        if (fs::exists(sourceTreePath / "sources" / "boot.wim")) {
            targetWimPath = sourceTreePath / "sources" / "boot.wim";
        } else if (fs::exists(sourceTreePath / "winpe.wim")) {
            targetWimPath = sourceTreePath / "winpe.wim";
        }
    } else {
        Logger::Log(LogLevel::Error, "Invalid source path provided.");
        return 1;
    }

    // DISM Servicing Workflow
    if (needsDism) {
        if (targetWimPath.empty() || !fs::exists(targetWimPath)) {
            Logger::Log(LogLevel::Error, "Cannot perform DISM servicing: boot.wim missing in source tree.");
            return 1;
        }

        ScopedTempDir mountDirGuard;
        fs::path activeMountPath = config.mountPath.empty() ? mountDirGuard.GetPath() : config.mountPath;

        if (!WinPEManager::CustomizeImage(targetWimPath, activeMountPath, config)) {
            Logger::Log(LogLevel::Error, "WinPE WIM servicing failed.");
            return 1;
        }
    }

    // ISO Builder Workflow
    if (!IsoBuilder::BuildIso(sourceTreePath, config.bootFilePath, config.outputIsoPath, config.volumeLabel)) {
        Logger::Log(LogLevel::Error, "ISO generation failed.");
        return 1;
    }

    Logger::Log(LogLevel::Info, "=== Operation finished successfully ===");
    return 0;
}