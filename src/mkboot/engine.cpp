#include "engine.hpp"

std::atomic<bool> g_AbortRequested{ false };
bool Logger::s_Verbose = false;

BOOL WINAPI ConsoleControlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        g_AbortRequested.store(true);
        return TRUE;
    }
    return FALSE;
}

void Logger::SetVerbose(bool enable) {
    s_Verbose = enable;
}

void Logger::Log(LogLevel level, const std::string& message) {
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
    return true;
}

ScopedCOM::ScopedCOM() {
    m_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

ScopedCOM::~ScopedCOM() {
    if (SUCCEEDED(m_hr)) {
        CoUninitialize();
    }
}

bool ScopedCOM::IsSucceeded() const { return SUCCEEDED(m_hr); }
HRESULT ScopedCOM::GetResult() const { return m_hr; }

#if MKBOOT_HAS_DISM
ScopedDism::ScopedDism() {
    m_hr = DismInitialize(DismLogErrorsWarningsInfo, L"mkboot_dism.log", NULL);
}
ScopedDism::~ScopedDism() {
    if (SUCCEEDED(m_hr)) {
        DismShutdown();
    }
}
bool ScopedDism::IsSucceeded() const { return SUCCEEDED(m_hr); }
HRESULT ScopedDism::GetResult() const { return m_hr; }
#else
ScopedDism::ScopedDism() : m_hr(E_FAIL) {}
ScopedDism::~ScopedDism() {}
bool ScopedDism::IsSucceeded() const { return false; }
HRESULT ScopedDism::GetResult() const { return m_hr; }
#endif

ScopedTempDir::ScopedTempDir() {
    wchar_t tempPathBuffer[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPathBuffer);
    wchar_t uniquePath[MAX_PATH];
    GetTempFileNameW(tempPathBuffer, L"MKB", 0, uniquePath);

    m_Path = uniquePath;
    fs::remove(m_Path);
    fs::create_directories(m_Path);
}

ScopedTempDir::~ScopedTempDir() {
    try {
        if (fs::exists(m_Path)) {
            fs::remove_all(m_Path);
        }
    } catch (...) {
    }
}

const fs::path& ScopedTempDir::GetPath() const { return m_Path; }

// ============================================================================
// DISM WINPE SERVICING ENGINE
// ============================================================================
bool WinPEManager::CustomizeImage(const fs::path& wimPath, const fs::path& mountDir, const ConfigOptions& config) {
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

    if (g_AbortRequested.load()) {
        Logger::Log(LogLevel::Warning, "Abort requested prior to WIM commit. Discarding changes...");
        success = false;
    }

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

// ============================================================================
// HARDENED IMAPI2 ISO BUILDER
// ============================================================================
bool IsoBuilder::BuildIso(const fs::path& sourceTree, const fs::path& bootFileOverride, const fs::path& outputIso, std::string volumeLabel) {
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

    Logger::Log(LogLevel::Info, "Writing ISO image to disk: " + outputIso.string());
    return WriteStreamToFile(pImageStream.Get(), outputIso);
}

bool IsoBuilder::WriteStreamToFile(IStream* pStream, const fs::path& outputPath) {
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        Logger::Log(LogLevel::Error, "Failed opening output ISO file stream.");
        return false;
    }

    const ULONG bufferSize = 64 * 1024;
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
