#include "reporter.hpp"
#include <iostream>
#include <vector>
#include <iomanip>

std::wstring MachinfoReporter::formatMemorySize(ULONGLONG bytes) {
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    double gb = mb / 1024.0;
    wchar_t buf[64];
    if (gb >= 1.0) {
        swprintf_s(buf, 64, L"%.0f MB (%.2f GB)", mb, gb);
    } else {
        swprintf_s(buf, 64, L"%.0f MB", mb);
    }
    return std::wstring(buf);
}

std::wstring MachinfoReporter::jsonEscape(const std::wstring& value) {
    std::wstring out;
    for (wchar_t c : value) {
        if (c == L'"' || c == L'\\') out += L'\\';
        if (c == L'\n') out += L"\\n";
        else out += c;
    }
    return out;
}

void MachinfoReporter::report(const CpuTopology& cpu, const SystemFirmwareInfo& fw, const MemoryStatusInfo& mem,
                              const TpmInfo& tpm, const std::wstring& osVersion, const MachinfoOptions& opts) {
    const bool hasSpecificSection = opts.showBios || opts.showCpu || opts.showMemory || opts.showTpm;
    const bool displayAll = opts.showAll || !hasSpecificSection;
    const bool displayBios = displayAll || opts.showBios;
    const bool displayCpu = displayAll || opts.showCpu;
    const bool displayMemory = displayAll || opts.showMemory;
    const bool displayTpm = displayAll || opts.showTpm;

    if (opts.format == MachinfoOptions::OutputFormat::Json) {
        std::wcout << L"{\n";
        std::vector<std::wstring> fields;
        if (displayAll || displayBios) {
            fields.push_back(L"  \"manufacturer\":\"" + jsonEscape(fw.manufacturer) + L"\"");
            fields.push_back(L"  \"product\":\"" + jsonEscape(fw.productName) + L"\"");
            fields.push_back(L"  \"serial\":\"" + jsonEscape(fw.serialNumber) + L"\"");
            if (!displayAll) {
                fields.push_back(L"  \"firmwareType\":\"" + jsonEscape(fw.firmwareType) + L"\"");
                fields.push_back(L"  \"biosVendor\":\"" + jsonEscape(fw.biosVendor) + L"\"");
                fields.push_back(L"  \"biosVersion\":\"" + jsonEscape(fw.biosVersion) + L"\"");
            }
        }
        if (displayAll || displayCpu) {
            fields.push_back(L"  \"cpu\":\"" + jsonEscape(cpu.name) + L"\"");
        }
        if (displayAll || displayMemory) {
            fields.push_back(L"  \"memory\":\"" + jsonEscape(formatMemorySize(mem.totalPhysBytes)) + L"\"");
        }
        if (displayAll) {
            fields.push_back(L"  \"os\":\"" + jsonEscape(osVersion) + L"\"");
        }
        if (displayAll || displayTpm) {
            fields.push_back(L"  \"tpmPresent\":" + std::wstring(tpm.present ? L"true" : L"false"));
            if (!displayAll && tpm.specVersion != L"N/A") {
                fields.push_back(L"  \"tpmSpecVersion\":\"" + jsonEscape(tpm.specVersion) + L"\"");
            }
        }
        for (size_t i = 0; i < fields.size(); ++i) {
            std::wcout << fields[i] << (i + 1 < fields.size() ? L",\n" : L"\n");
        }
        std::wcout << L"}\n";
        return;
    }

    if (opts.format == MachinfoOptions::OutputFormat::Csv) {
        std::wcout << L"Field,Value\n";
        if (displayAll || displayBios) {
            std::wcout << L"Manufacturer,\"" << fw.manufacturer << L"\"\n"
                      << L"Product,\"" << fw.productName << L"\"\n";
            if (!displayAll) {
                std::wcout << L"BIOS Vendor,\"" << fw.biosVendor << L"\"\n"
                          << L"BIOS Version,\"" << fw.biosVersion << L"\"\n";
            }
        }
        if (displayAll || displayCpu) {
            std::wcout << L"CPU,\"" << cpu.name << L"\"\n";
        }
        if (displayAll || displayMemory) {
            std::wcout << L"Memory,\"" << formatMemorySize(mem.totalPhysBytes) << L"\"\n";
        }
        if (displayAll) {
            std::wcout << L"OS,\"" << osVersion << L"\"\n";
        }
        if (displayAll || displayTpm) {
            std::wcout << L"TPM Present,\"" << (tpm.present ? L"Yes" : L"No") << L"\"\n";
        }
        return;
    }

    if (opts.quiet) {
        if (displayAll) {
            std::wcout << L"Host:     " << fw.productName << L" (" << fw.manufacturer << L")\n";
        }
        if (displayCpu) {
            std::wcout << L"CPU:      " << cpu.name << L" (" << cpu.coreCount << L" Cores, " << cpu.logicalCount << L" Threads)\n";
        }
        if (displayMemory) {
            std::wcout << L"Memory:   " << formatMemorySize(mem.totalPhysBytes) << L"\n";
        }
        if (displayBios) {
            std::wcout << L"Firmware: " << fw.firmwareType << L" " << fw.biosVersion << L"\n";
        }
        if (displayTpm) {
            std::wcout << L"TPM:      " << (tpm.present ? (tpm.specVersion == L"N/A" ? L"Present" : (L"Present (" + tpm.specVersion + L")")) : L"Not Detected") << L"\n";
        }
        if (displayAll) {
            std::wcout << L"OS:       " << osVersion << L"\n";
        }
        return;
    }

    std::wcout << L"\nSystem and Hardware Summary v10.9.0\n\n";

    if (displayAll) {
        std::wcout << L"Machine Info:\n"
                   << L"    Manufacturer:       " << fw.manufacturer << L"\n"
                   << L"    Product Name:       " << fw.productName << L"\n"
                   << L"    Serial Number:      " << fw.serialNumber << L"\n"
                   << L"    UUID:               " << fw.uuid << L"\n"
                   << L"    Architecture:       64-bit x86-64 Architecture\n\n";
    }

    if (displayBios) {
        std::wcout << L"Firmware / BIOS Info:\n"
                   << L"    Firmware Type:      " << fw.firmwareType << L"\n"
                   << L"    BIOS Vendor:        " << fw.biosVendor << L"\n"
                   << L"    BIOS Version:       " << fw.biosVersion << L"\n"
                   << L"    Release Date:       " << fw.biosReleaseDate << L"\n\n";
    }

    if (displayTpm) {
        std::wcout << L"TPM / Security Info:\n"
                   << L"    TPM Present:        " << (tpm.present ? L"Yes" : L"No") << L"\n"
                   << L"    TPM Ready:          " << (tpm.ready ? L"Yes" : L"No") << L"\n"
                   << L"    TPM Spec Version:   " << tpm.specVersion << L"\n"
                   << L"    Manufacturer:       " << tpm.manufacturer << L"\n"
                   << L"    Manufacturer Ver:   " << tpm.manufacturerVersion << L"\n\n";
    }

    if (displayCpu) {
        std::wcout << L"Processor Info:\n"
                   << L"    Model:              " << cpu.name << L"\n"
                   << L"    Clock Speed:        " << cpu.clockMhz << L" MHz\n"
                   << L"    Sockets:            " << (cpu.socketCount > 0 ? cpu.socketCount : 1) << L"\n"
                   << L"    Physical Cores:     " << cpu.coreCount << L"\n"
                   << L"    Logical Processors: " << cpu.logicalCount << L"\n"
                   << L"    NUMA Nodes:         " << (cpu.numaNodeCount > 0 ? cpu.numaNodeCount : 1) << L"\n";

        if (opts.verbose) {
            std::wcout << L"    L1 Cache:           " << formatMemorySize(cpu.l1CacheBytes) << L"\n"
                       << L"    L2 Cache:           " << formatMemorySize(cpu.l2CacheBytes) << L"\n"
                       << L"    L3 Cache:           " << formatMemorySize(cpu.l3CacheBytes) << L"\n"
                       << L"    Hardware VT:        " << (cpu.supportsVirtualization ? L"Enabled" : L"Disabled / Unavailable") << L"\n";
        }
        std::wcout << L"\n";
    }

    if (displayMemory) {
        std::wcout << L"Memory Info:\n"
                   << L"    Total Physical:     " << formatMemorySize(mem.totalPhysBytes) << L"\n"
                   << L"    Available Physical: " << formatMemorySize(mem.availPhysBytes) << L"\n"
                   << L"    Memory Utilization: " << mem.loadPercentage << L"%\n"
                   << L"    Commit Limit:       " << formatMemorySize(mem.pageFileLimitBytes) << L"\n\n";
    }

    if (displayAll) {
        std::wcout << L"Operating System Info:\n"
                   << L"    OS Name:            " << osVersion << L"\n";
    }
}
