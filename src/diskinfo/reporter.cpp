#include "reporter.hpp"
#include "engine.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <unordered_map>

namespace {
struct TableRow {
    size_t index;
    std::wstring name;
    std::wstring mountPoints;
};

std::wstring ReadOnlyValue(const BlockDeviceRow& row) {
    if (row.readOnly == L"yes") return L"1";
    if (row.readOnly == L"no") return L"0";
    return L"-";
}

std::wstring RemovableValue(const BlockDeviceRow& row) {
    return row.type == L"removable" || row.type == L"rom" ? L"1" : L"0";
}

std::wstring MountPointsValue(const BlockDeviceRow& row) {
    std::wstring value = row.mountPoint;
    size_t separator = 0;
    while ((separator = value.find(L';', separator)) != std::wstring::npos) {
        value.replace(separator, 1, L", ");
        separator += 2;
    }
    return value;
}

bool IsRepresentedByPartition(const BlockDeviceRow& row,
                              const std::vector<BlockDeviceRow>& rows) {
    if (row.type != L"volume" || row.mountPoint.empty()) return false;
    return std::any_of(rows.begin(), rows.end(), [&row](const BlockDeviceRow& candidate) {
        return candidate.type == L"part" && candidate.parent == row.parent &&
               candidate.mountPoint == row.mountPoint;
    });
}

std::vector<TableRow> BuildTableRows(const std::vector<BlockDeviceRow>& rows) {
    std::unordered_map<std::wstring, size_t> firstIndexByName;
    for (size_t i = 0; i < rows.size(); ++i) {
        firstIndexByName.emplace(rows[i].name, i);
    }

    std::vector<std::vector<size_t>> children(rows.size());
    std::vector<size_t> roots;
    for (size_t i = 0; i < rows.size(); ++i) {
        auto parent = firstIndexByName.find(rows[i].parent);
        if (!rows[i].parent.empty() && parent != firstIndexByName.end() && parent->second != i) {
            children[parent->second].push_back(i);
        } else {
            roots.push_back(i);
        }
    }

    std::vector<TableRow> tableRows;
    std::vector<bool> visited(rows.size(), false);
    std::function<void(size_t, const std::wstring&, bool, bool)> append =
        [&](size_t index, const std::wstring& prefix, bool isLast, bool isRoot) {
            if (visited[index]) return;
            visited[index] = true;

            std::wstring name;
            if (!isRoot) {
                name = prefix + (isLast ? L"└─" : L"├─") + rows[index].name;
            } else {
                name = rows[index].name;
            }
            tableRows.push_back({index, std::move(name), MountPointsValue(rows[index])});

            const std::wstring childPrefix = isRoot
                ? L""
                : prefix + (isLast ? L"  " : L"│ ");
            for (size_t child = 0; child < children[index].size(); ++child) {
                append(children[index][child], childPrefix,
                       child + 1 == children[index].size(), false);
            }
        };

    for (size_t root : roots) append(root, L"", true, true);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (!visited[i]) append(i, L"", true, true);
    }
    return tableRows;
}
}

void DiskinfoReporter::Report(const std::vector<BlockDeviceRow>& rows, const DiskinfoOptions& opts) {
    std::vector<BlockDeviceRow> selectedRows;
    for (const auto& row : rows) {
        bool keep = opts.filters.empty();
        for (const auto& filter : opts.filters) {
            if (StringHelper::ContainsICase(row.name, filter) ||
                StringHelper::ContainsICase(row.type, filter) ||
                StringHelper::ContainsICase(row.parent, filter) ||
                StringHelper::ContainsICase(row.fsType, filter) ||
                StringHelper::ContainsICase(row.mountPoint, filter) ||
                StringHelper::ContainsICase(row.size, filter) ||
                StringHelper::ContainsICase(row.readOnly, filter)) {
                keep = true;
                break;
            }
        }
        if (keep) selectedRows.push_back(row);
    }

    if (opts.format == OutputFormat::Csv) {
        if (!opts.noHeadings) {
            std::wcout << L"NAME,TYPE,PKNAME,FSTYPE,MOUNTPOINT,SIZE_BYTES,SIZE,RO\n";
        }
        for (const auto& row : selectedRows) {
            std::wcout << StringHelper::CsvBlk(row.name) << L','
                       << StringHelper::CsvBlk(row.type) << L','
                       << StringHelper::CsvBlk(row.parent) << L','
                       << StringHelper::CsvBlk(row.fsType) << L','
                       << StringHelper::CsvBlk(row.mountPoint) << L','
                       << (row.sizeKnown ? std::to_wstring(row.sizeBytes) : L"") << L','
                       << StringHelper::CsvBlk(row.size) << L','
                       << StringHelper::CsvBlk(row.readOnly) << L'\n';
        }
        return;
    }

    if (opts.format == OutputFormat::Json) {
        std::wcout << L"[\n";
        for (size_t i = 0; i < selectedRows.size(); ++i) {
            const auto& row = selectedRows[i];
            std::wcout << L"  {\"name\":\"" << StringHelper::JsonBlk(row.name)
                       << L"\",\"type\":\"" << StringHelper::JsonBlk(row.type)
                       << L"\",\"parent\":\"" << StringHelper::JsonBlk(row.parent)
                       << L"\",\"fsType\":\"" << StringHelper::JsonBlk(row.fsType)
                       << L"\",\"mountPoint\":\"" << StringHelper::JsonBlk(row.mountPoint)
                       << L"\",\"sizeBytes\":";
            if (row.sizeKnown) std::wcout << row.sizeBytes;
            else std::wcout << L"null";
            std::wcout << L",\"size\":\"" << StringHelper::JsonBlk(row.size)
                       << L"\",\"readOnly\":";
            if (row.readOnly == L"yes") std::wcout << L"true";
            else if (row.readOnly == L"no") std::wcout << L"false";
            else std::wcout << L"null";
            std::wcout << L"}" << (i + 1 == selectedRows.size() ? L"\n" : L",\n");
        }
        std::wcout << L"]\n";
        return;
    }

    std::vector<BlockDeviceRow> tableSource;
    for (const auto& row : selectedRows) {
        if (!IsRepresentedByPartition(row, selectedRows)) tableSource.push_back(row);
    }

    const std::vector<TableRow> tableRows = BuildTableRows(tableSource);
    size_t nameWidth = 4;
    size_t sizeWidth = 4;
    size_t typeWidth = 4;
    for (const auto& tableRow : tableRows) {
        nameWidth = (std::max)(nameWidth, tableRow.name.size());
        sizeWidth = (std::max)(sizeWidth, tableSource[tableRow.index].size.size());
        typeWidth = (std::max)(typeWidth, tableSource[tableRow.index].type.size());
    }

    if (!opts.noHeadings) {
        std::wcout << std::left
                   << std::setw(static_cast<int>(nameWidth + 2)) << L"NAME"
                   << std::setw(8) << L"MAJ:MIN"
                   << std::setw(3) << L"RM"
                   << std::setw(static_cast<int>(sizeWidth + 2)) << L"SIZE"
                   << std::setw(3) << L"RO"
                   << std::setw(static_cast<int>(typeWidth + 2)) << L"TYPE"
                   << L"MOUNTPOINTS\n";
    }

    for (const auto& tableRow : tableRows) {
        const auto& row = tableSource[tableRow.index];
        std::wcout << std::left
                   << std::setw(static_cast<int>(nameWidth + 2)) << tableRow.name
                   << std::setw(8) << L"-"
                   << std::setw(3) << RemovableValue(row)
                   << std::setw(static_cast<int>(sizeWidth + 2))
                   << (row.sizeKnown ? row.size : L"-")
                   << std::setw(3) << ReadOnlyValue(row)
                   << std::setw(static_cast<int>(typeWidth + 2)) << row.type
                   << tableRow.mountPoints << L"\n";
    }
}
