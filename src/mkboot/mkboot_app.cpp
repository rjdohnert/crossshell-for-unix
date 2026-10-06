#include "mkboot_app.hpp"

int MkBootApp::Run(int argc, char* argv[]) {
    SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);

    ConfigOptions config;
    if (!ParseCommandLine(argc, argv, config)) {
        return 1;
    }

    if (config.showHelp || argc == 1) {
        PrintHelp(argv[0]);
        return 0;
    }

    if (config.sourcePath.empty()) {
        Logger::Log(LogLevel::Error, "Source path (-s, --source) is required.");
        return 1;
    }

    if (config.outputIsoPath.empty()) {
        Logger::Log(LogLevel::Error, "Output ISO path (-o, --output) is required.");
        return 1;
    }

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

    ULONGLONG estimatedBytesNeeded = 500 * 1024 * 1024;
    if (fs::is_regular_file(config.sourcePath)) {
        estimatedBytesNeeded = fs::file_size(config.sourcePath) * 3;
    }

    if (!HasSufficientDiskSpace(config.outputIsoPath, estimatedBytesNeeded)) {
        Logger::Log(LogLevel::Error, "Insufficient free disk space available for ISO compilation.");
        return 1;
    }

    ScopedCOM comGuard;
    if (!comGuard.IsSucceeded()) {
        Logger::Log(LogLevel::Error, "Failed initializing COM subsystem: " + GetHResultErrorMessage(comGuard.GetResult()));
        return 1;
    }

    ScopedDism dismGuard;
    if (!dismGuard.IsSucceeded()) {
        Logger::Log(LogLevel::Error, "Failed initializing DISM framework: " + GetHResultErrorMessage(dismGuard.GetResult()));
        return 1;
    }

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

    if (!IsoBuilder::BuildIso(sourceTreePath, config.bootFilePath, config.outputIsoPath, config.volumeLabel)) {
        Logger::Log(LogLevel::Error, "ISO generation failed.");
        return 1;
    }

    Logger::Log(LogLevel::Info, "=== Operation finished successfully ===");
    return 0;
}
