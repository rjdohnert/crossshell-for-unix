/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_targets.hpp"

#include "dskctl_helpers.hpp"

#include <knownfolders.h>

namespace fs = std::filesystem;

bool BuildCleanupTargets(std::vector<CleanupTarget>& targets,
                         std::string& error) {
    fs::path localAppData = GetKnownFolderPath(FOLDERID_LocalAppData, error);
    if (localAppData.empty()) return false;
    fs::path programData = GetKnownFolderPath(FOLDERID_ProgramData, error);
    if (programData.empty()) return false;
    fs::path windows = GetWindowsDirectoryPath(error);
    if (windows.empty()) return false;

    targets = {
        {"user-temp", "--temp", "User Temporary Files",
         "Cleans the current user's LocalAppData\\Temp directory.",
         {localAppData / L"Temp"}, RiskLevel::Low},
        {"system-temp", "--system-temp", "System Temporary Files",
         "Cleans Windows\\Temp.", {windows / L"Temp"}, RiskLevel::Medium},
        {"update-cache", "--update-cache", "Windows Update Cache",
         "Cleans downloaded Windows Update packages.",
         {windows / L"SoftwareDistribution" / L"Download"}, RiskLevel::Medium},
        {"error-dumps", "--error-dumps", "Memory Crash Dumps",
         "Cleans MEMORY.DMP and Minidump files.",
         {windows / L"MEMORY.DMP", windows / L"Minidump"}, RiskLevel::High},
        {"logs", "--logs", "Windows Setup and System Logs",
         "Cleans Windows\\Logs and Windows\\Panther.",
         {windows / L"Logs", windows / L"Panther"}, RiskLevel::High},
        {"recycle-bin", "--recycle-bin", "Recycle Bin",
         "Empties the current user's Recycle Bin.", {}, RiskLevel::High, true},
        {"wer", "--wer", "Windows Error Reports",
         "Cleans user crash dumps and Windows Error Reporting files.",
         {localAppData / L"CrashDumps",
          programData / L"Microsoft" / L"Windows" / L"WER"}, RiskLevel::High},
        {"shaders", "--shaders", "DirectX Shader Caches",
         "Cleans known GPU and DirectX shader-cache locations.",
         {localAppData / L"D3DSCache",
          localAppData / L"NVIDIA" / L"DXCache",
          localAppData / L"AMD" / L"DxCache"}, RiskLevel::Medium},
        {"delivery-opt", "--delivery-opt", "Delivery Optimization Cache",
         "Cleans the Windows Delivery Optimization cache.",
         {programData / L"Microsoft" / L"Windows" /
          L"DeliveryOptimization" / L"Cache"}, RiskLevel::Medium},
        {"inet-cache", "--inet-cache", "Internet Cache",
         "Cleans the current user's INetCache directory.",
         {localAppData / L"Microsoft" / L"Windows" / L"INetCache"},
         RiskLevel::Medium}
    };
    return true;
}

std::vector<fs::path> GetApprovedCleanupRoots(
    const std::vector<CleanupTarget>& targets) {
    std::vector<fs::path> roots;
    for (const CleanupTarget& target : targets) {
        roots.insert(roots.end(), target.paths.begin(), target.paths.end());
    }
    return roots;
}
