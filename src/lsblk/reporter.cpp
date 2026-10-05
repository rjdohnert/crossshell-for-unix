#include "reporter.hpp"
#include "engine.hpp"
#include <iostream>
#include <iomanip>

void LsblkReporter::Report(const std::vector<BlockDeviceRow>& rows, const LsblkOptions& opts) {
    std::vector<BlockDeviceRow> selectedRows;
    for (const auto& row : rows) {
        bool keep = opts.filters.empty();
        for (const auto& filter : opts.filters) {
            if (StringHelper::ContainsICase(row.name, filter) ||
                StringHelper::ContainsICase(row.type, filter) ||
                StringHelper::ContainsICase(row.fsType, filter) ||
                StringHelper::ContainsICase(row.mountPoint, filter) ||
                StringHelper::ContainsICase(row.size, filter) ||
                StringHelper::ContainsICase(row.readOnly, filter) ||
                StringHelper::ContainsICase(row.health, filter)) {
                keep = true;
                break;
            }
        }
        if (keep) selectedRows.push_back(row);
    }

    if (opts.format == OutputFormat::Csv) {
        std::wcout << L"NAME,TYPE,FSTYPE,MOUNTPOINT,SIZE,RO,HEALTH\n";
        for (const auto& row : selectedRows) {
            std::cout << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.name)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.type)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.fsType)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.mountPoint)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.size)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.readOnly)) << ','
                      << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.health)) << '\n';
        }
        return;
    }

    if (opts.format == OutputFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < selectedRows.size(); ++i) {
            const auto& row = selectedRows[i];
            std::cout << "  {\"name\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.name))
                      << "\",\"type\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.type))
                      << "\",\"fsType\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.fsType))
                      << "\",\"mountPoint\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.mountPoint))
                      << "\",\"size\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.size))
                      << "\",\"readOnly\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.readOnly))
                      << "\",\"health\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.health))
                      << "\"}" << (i + 1 == selectedRows.size() ? "\n" : ",\n");
        }
        std::cout << "]\n";
        return;
    }

    if (!opts.noHeadings) {
        std::wcout << std::left
                   << std::setw(18) << L"NAME"
                   << std::setw(12) << L"TYPE"
                   << std::setw(14) << L"FSTYPE"
                   << std::setw(18) << L"MOUNTPOINT"
                   << std::setw(12) << L"SIZE"
                   << std::setw(12) << L"RO"
                   << L"HEALTH\n";
    }

    for (const auto& row : selectedRows) {
        std::wcout << std::left
                   << std::setw(18) << row.name
                   << std::setw(12) << row.type
                   << std::setw(14) << row.fsType
                   << std::setw(18) << row.mountPoint
                   << std::setw(12) << row.size
                   << std::setw(12) << row.readOnly
                   << row.health << L"\n";
    }
}
