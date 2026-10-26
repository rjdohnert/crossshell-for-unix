#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <sstream>

int main() {
    const std::wstring rawCsv = L"share, \"quoted\"\r\nline";
    const std::wstring expectedCsv = L"\"share, \"\"quoted\"\"\r\nline\"";
    if (StringHelper::CsvBlk(rawCsv) != expectedCsv) return 1;

    const std::wstring rawJson = L"unicode 雪 quote\" slash\\ line\n tab\t control\x0002";
    const std::wstring expectedJson = L"unicode 雪 quote\\\" slash\\\\ line\\n tab\\t control\\u0002";
    if (StringHelper::JsonBlk(rawJson) != expectedJson) return 2;

    BlockDeviceRow disk;
    disk.name = L"PhysicalDrive0";
    disk.type = L"disk";
    disk.size = L"476.9G";
    disk.sizeKnown = true;
    disk.readOnly = L"no";

    BlockDeviceRow partition;
    partition.name = L"PhysicalDrive0p1";
    partition.type = L"part";
    partition.parent = disk.name;
    partition.size = L"476.9G";
    partition.sizeKnown = true;
    partition.readOnly = L"no";
    partition.mountPoint = L"C:\\";

    BlockDeviceRow volume;
    volume.name = L"C:";
    volume.type = L"volume";
    volume.parent = disk.name;
    volume.size = L"476.9G";
    volume.sizeKnown = true;
    volume.readOnly = L"no";
    volume.mountPoint = L"C:\\";

    std::wostringstream tableOutput;
    std::wstreambuf* originalOutput = std::wcout.rdbuf(tableOutput.rdbuf());
    DiskinfoOptions options;
    DiskinfoReporter::Report({disk, partition, volume}, options);
    std::wcout.rdbuf(originalOutput);
    const std::wstring table = tableOutput.str();
    if (table.find(L"NAME") == std::wstring::npos ||
        table.find(L"MAJ:MIN") == std::wstring::npos ||
        table.find(L"MOUNTPOINTS") == std::wstring::npos) return 3;
    if (table.find(L"└─PhysicalDrive0p1") == std::wstring::npos ||
        table.find(L"C:\\") == std::wstring::npos) return 4;
    const size_t mountPointPosition = table.find(L"C:\\");
    if (table.find(L"C:\\", mountPointPosition + 1) != std::wstring::npos) return 5;

    std::wostringstream noHeaderOutput;
    originalOutput = std::wcout.rdbuf(noHeaderOutput.rdbuf());
    options.noHeadings = true;
    DiskinfoReporter::Report({disk, partition}, options);
    std::wcout.rdbuf(originalOutput);
    if (noHeaderOutput.str().find(L"MAJ:MIN") != std::wstring::npos) return 6;
    return 0;
}
