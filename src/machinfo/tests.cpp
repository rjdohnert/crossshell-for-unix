#include "engine.hpp"
#include "options.hpp"
#include "reporter.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

int main() {
    const std::wstring rawJson = L"quote\" slash\\ newline\n tab\t backspace\b formfeed\f control\x0001";
    const std::wstring expectedJson = L"quote\\\" slash\\\\ newline\\n tab\\t backspace\\b formfeed\\f control\\u0001";
    if (MachinfoReporter::jsonEscape(rawJson) != expectedJson) return 1;

    const std::wstring rawCsv = L"alpha,\"beta\"\r\n";
    const std::wstring expectedCsv = L"\"alpha,\"\"beta\"\"\r\n\"";
    if (MachinfoReporter::csvEscape(rawCsv) != expectedCsv) return 2;

    std::vector<BYTE> table(0x19, 0);
    table[0] = 1;
    table[1] = 0x19;
    table[4] = 1;
    table[5] = 2;
    table[7] = 3;
    const char strings[] = "Vendor\0Product\0Serial\0\0";
    table.insert(table.end(), strings, strings + sizeof(strings) - 1);
    const SystemFirmwareInfo firmware = SmbiosParser::parseSmbiosTable(table.data(), table.size());
    if (firmware.manufacturer != L"Vendor" || firmware.productName != L"Product" ||
        firmware.serialNumber != L"Serial") return 3;

    const BYTE malformedTable[] = {1, 3, 0, 0};
    const SystemFirmwareInfo malformed = SmbiosParser::parseSmbiosTable(
        malformedTable, sizeof(malformedTable));
    if (malformed.manufacturer != L"N/A" || malformed.productName != L"N/A") return 4;
    const SystemFirmwareInfo empty = SmbiosParser::parseSmbiosTable(nullptr, 0);
    if (empty.manufacturer != L"N/A") return 5;

    MachinfoOptions memoryJsonOptions;
    memoryJsonOptions.showMemory = true;
    memoryJsonOptions.format = MachinfoOptions::OutputFormat::Json;
    MemoryStatusInfo unavailableMemory;
    CpuTopology emptyCpu;
    SystemFirmwareInfo emptyFirmware;
    TpmInfo emptyTpm;
    std::wostringstream jsonOutput;
    std::wstreambuf* originalOutput = std::wcout.rdbuf(jsonOutput.rdbuf());
    MachinfoReporter::report(emptyCpu, emptyFirmware, unavailableMemory, emptyTpm,
                             L"test", memoryJsonOptions);
    std::wcout.rdbuf(originalOutput);
    if (jsonOutput.str().find(L"\"memory_total_physical_bytes\":null") == std::wstring::npos) return 6;

    MachinfoOptions memoryCsvOptions = memoryJsonOptions;
    memoryCsvOptions.format = MachinfoOptions::OutputFormat::Csv;
    std::wostringstream csvOutput;
    originalOutput = std::wcout.rdbuf(csvOutput.rdbuf());
    MachinfoReporter::report(emptyCpu, emptyFirmware, unavailableMemory, emptyTpm,
                             L"test", memoryCsvOptions);
    std::wcout.rdbuf(originalOutput);
    if (csvOutput.str().find(L"\"memory_total_physical_bytes\",\"N/A\"") == std::wstring::npos) return 7;

    wchar_t program[] = L"machinfo";
    wchar_t format[] = L"--format=json";
    wchar_t cpu[] = L"cpu";
    wchar_t operatingSystem[] = L"os";
    wchar_t* validArgs[] = {program, format, cpu, operatingSystem};
    MachinfoOptions options;
    if (!MachinfoOptions::parse(4, validArgs, options)) return 8;
    if (options.format != MachinfoOptions::OutputFormat::Json || !options.showCpu ||
        !options.showOs || options.showAll) return 9;

    wchar_t outputOption[] = L"--output";
    wchar_t invalidFormat[] = L"xml";
    wchar_t* invalidArgs[] = {program, outputOption, invalidFormat};
    MachinfoOptions invalidOptions;
    if (MachinfoOptions::parse(3, invalidArgs, invalidOptions)) return 10;

    wchar_t missingFormat[] = L"--format";
    wchar_t* missingArgs[] = {program, missingFormat};
    MachinfoOptions missingOptions;
    if (MachinfoOptions::parse(2, missingArgs, missingOptions)) return 11;
    return 0;
}