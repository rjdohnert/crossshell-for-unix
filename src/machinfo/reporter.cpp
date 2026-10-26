#include "reporter.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <utility>

namespace {
enum class JsonValueType { String, Number, Boolean, Null };

struct StructuredField {
    std::wstring name;
    std::wstring value;
    JsonValueType type;
};

std::wstring numberValue(ULONGLONG value) {
    return std::to_wstring(value);
}
}

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
    static constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring out;
    for (wchar_t c : value) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\b': out += L"\\b"; break;
            case L'\f': out += L"\\f"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c >= 0 && c < 0x20) {
                    const unsigned int code = static_cast<unsigned int>(c);
                    out += L"\\u00";
                    out += hex[(code >> 4) & 0xF];
                    out += hex[code & 0xF];
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::wstring MachinfoReporter::csvEscape(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size() + 2);
    out += L'"';
    for (wchar_t c : value) {
        if (c == L'"') out += L'"';
        out += c;
    }
    out += L'"';
    return out;
}

void MachinfoReporter::report(const CpuTopology& cpu, const SystemFirmwareInfo& fw, const MemoryStatusInfo& mem,
                              const TpmInfo& tpm, const std::wstring& osVersion, const MachinfoOptions& opts) {
    const bool hasSpecificSection = opts.showBios || opts.showCpu || opts.showMemory || opts.showTpm || opts.showOs;
    const bool displayAll = opts.showAll || !hasSpecificSection;
    const bool displayBios = displayAll || opts.showBios;
    const bool displayCpu = displayAll || opts.showCpu;
    const bool displayMemory = displayAll || opts.showMemory;
    const bool displayTpm = displayAll || opts.showTpm;
    const bool displayOs = displayAll || opts.showOs;

    std::vector<StructuredField> fields;
    auto addString = [&fields](const wchar_t* name, const std::wstring& value) {
        fields.push_back({name, value, JsonValueType::String});
    };
    auto addNumber = [&fields](const wchar_t* name, ULONGLONG value) {
        fields.push_back({name, numberValue(value), JsonValueType::Number});
    };
    auto addBoolean = [&fields](const wchar_t* name, bool value) {
        fields.push_back({name, value ? L"true" : L"false", JsonValueType::Boolean});
    };

    if (displayBios) {
        addString(L"manufacturer", fw.manufacturer);
        addString(L"product", fw.productName);
        addString(L"serial", fw.serialNumber);
        addString(L"uuid", fw.uuid);
        addString(L"firmware_type", fw.firmwareType);
        addString(L"bios_vendor", fw.biosVendor);
        addString(L"bios_version", fw.biosVersion);
        addString(L"bios_release_date", fw.biosReleaseDate);
    }
    if (displayCpu) {
        addString(L"cpu_model", cpu.name);
        addNumber(L"cpu_clock_mhz", cpu.clockMhz);
        addNumber(L"cpu_sockets", cpu.socketCount);
        addNumber(L"cpu_cores", cpu.coreCount);
        addNumber(L"cpu_logical_processors", cpu.logicalCount);
        addNumber(L"cpu_numa_nodes", cpu.numaNodeCount);
        addString(L"cpu_architecture", cpu.architecture);
        addBoolean(L"cpu_virtualization_firmware_enabled", cpu.virtualizationFirmwareEnabled);
        addBoolean(L"cpu_avx2_supported", cpu.supportsAVX2);
        if (opts.verbose) {
            addNumber(L"cpu_l1_cache_bytes", cpu.l1CacheBytes);
            addNumber(L"cpu_l2_cache_bytes", cpu.l2CacheBytes);
            addNumber(L"cpu_l3_cache_bytes", cpu.l3CacheBytes);
        }
    }
    if (displayMemory) {
        if (mem.valid) {
            addNumber(L"memory_total_physical_bytes", mem.totalPhysBytes);
            addNumber(L"memory_available_physical_bytes", mem.availPhysBytes);
            fields.push_back({L"memory_load_percent", std::to_wstring(mem.loadPercentage), JsonValueType::Number});
            addNumber(L"memory_commit_limit_bytes", mem.pageFileLimitBytes);
        } else {
            fields.push_back({L"memory_total_physical_bytes", L"", JsonValueType::Null});
            fields.push_back({L"memory_available_physical_bytes", L"", JsonValueType::Null});
            fields.push_back({L"memory_load_percent", L"", JsonValueType::Null});
            fields.push_back({L"memory_commit_limit_bytes", L"", JsonValueType::Null});
        }
    }
    if (displayTpm) {
        addBoolean(L"tpm_present", tpm.present);
        addBoolean(L"tpm_ready", tpm.ready);
        addString(L"tpm_spec_version", tpm.specVersion);
        addString(L"tpm_manufacturer", tpm.manufacturer);
        addString(L"tpm_manufacturer_version", tpm.manufacturerVersion);
    }
    if (displayOs) addString(L"os_version", osVersion);

    if (opts.format == MachinfoOptions::OutputFormat::Json) {
        std::wcout << L"{\n";
        for (size_t i = 0; i < fields.size(); ++i) {
            const StructuredField& field = fields[i];
            std::wcout << L"  \"" << jsonEscape(field.name) << L"\":";
            if (field.type == JsonValueType::String) std::wcout << L"\"" << jsonEscape(field.value) << L"\"";
            else if (field.type == JsonValueType::Null) std::wcout << L"null";
            else std::wcout << field.value;
            std::wcout << (i + 1 < fields.size() ? L",\n" : L"\n");
        }
        std::wcout << L"}\n";
        return;
    }

    if (opts.format == MachinfoOptions::OutputFormat::Csv) {
        std::wcout << L"Field,Value\n";
        for (const StructuredField& field : fields) {
            const std::wstring value = field.type == JsonValueType::Null ? L"N/A" : field.value;
            std::wcout << csvEscape(field.name) << L"," << csvEscape(value) << L"\n";
        }
        return;
    }

    if (opts.quiet) {
        if (displayBios) {
            std::wcout << L"Host:     " << fw.productName << L" (" << fw.manufacturer << L")\n";
        }
        if (displayCpu) {
            std::wcout << L"CPU:      " << cpu.name << L" (" << cpu.coreCount << L" Cores, " << cpu.logicalCount << L" Threads)\n";
        }
        if (displayMemory) {
            std::wcout << L"Memory:   " << (mem.valid ? formatMemorySize(mem.totalPhysBytes) : L"N/A") << L"\n";
        }
        if (displayBios) {
            std::wcout << L"Firmware: " << fw.firmwareType << L" " << fw.biosVersion << L"\n";
        }
        if (displayTpm) {
            std::wcout << L"TPM:      " << (tpm.present ? (tpm.specVersion == L"N/A" ? L"Present" : (L"Present (" + tpm.specVersion + L")")) : L"Not Detected") << L"\n";
        }
        if (displayOs) {
            std::wcout << L"OS:       " << osVersion << L"\n";
        }
        return;
    }

    std::wcout << L"\nSystem and Hardware Summary v10.9.0\n\n";

    if (displayBios) {
        std::wcout << L"Machine Info:\n"
                   << L"    Manufacturer:       " << fw.manufacturer << L"\n"
                   << L"    Product Name:       " << fw.productName << L"\n"
                   << L"    Serial Number:      " << fw.serialNumber << L"\n"
                   << L"    UUID:               " << fw.uuid << L"\n"
                   << L"    Architecture:       " << cpu.architecture << L"\n\n";
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
                       << L"    Virtualization firmware: " << (cpu.virtualizationFirmwareEnabled ? L"Enabled" : L"Disabled / Unavailable") << L"\n"
                       << L"    AVX2:               " << (cpu.supportsAVX2 ? L"Supported" : L"Unavailable") << L"\n";
        }
        std::wcout << L"\n";
    }

    if (displayMemory) {
        const std::wstring total = mem.valid ? formatMemorySize(mem.totalPhysBytes) : L"N/A";
        const std::wstring available = mem.valid ? formatMemorySize(mem.availPhysBytes) : L"N/A";
        const std::wstring utilization = mem.valid ? std::to_wstring(mem.loadPercentage) + L"%" : L"N/A";
        const std::wstring commit = mem.valid ? formatMemorySize(mem.pageFileLimitBytes) : L"N/A";
        std::wcout << L"Memory Info:\n"
                   << L"    Total Physical:     " << total << L"\n"
                   << L"    Available Physical: " << available << L"\n"
                   << L"    Memory Utilization: " << utilization << L"\n"
                   << L"    Commit Limit:       " << commit << L"\n\n";
    }

    if (displayOs) {
        std::wcout << L"Operating System Info:\n"
                   << L"    OS Name:            " << osVersion << L"\n";
    }
}
