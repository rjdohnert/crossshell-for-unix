#include "reporter.hpp"

std::string OutputFormatter::FormatBytes(uint64_t bytes, DisplayUnit unit) const {
    if (unit == DisplayUnit::Bytes) return std::to_string(bytes) + " B";
    
    double val = static_cast<double>(bytes);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    switch (unit) {
        case DisplayUnit::Kibi: oss << (val / 1024.0) << " KiB"; return oss.str();
        case DisplayUnit::Mebi: oss << (val / (1024.0 * 1024.0)) << " MiB"; return oss.str();
        case DisplayUnit::Gibi: oss << (val / (1024.0 * 1024.0 * 1024.0)) << " GiB"; return oss.str();
        case DisplayUnit::Human:
        default:
            if (val >= 1024.0 * 1024.0 * 1024.0) {
                oss << (val / (1024.0 * 1024.0 * 1024.0)) << " GiB";
            } else if (val >= 1024.0 * 1024.0) {
                oss << (val / (1024.0 * 1024.0)) << " MiB";
            } else if (val >= 1024.0) {
                oss << (val / 1024.0) << " KiB";
            } else {
                oss << val << " B";
            }
            return oss.str();
    }
}

double OutputFormatter::Percent(uint64_t part, uint64_t total) const {
    return total == 0 ? 0.0 : (static_cast<double>(part) / total) * 100.0;
}

std::string OutputFormatter::Narrow(const std::wstring& wide) const {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

// --- TABLE FORMATTER ---
void TableFormatter::Render(std::ostream& os, const MemorySnapshot& s, const Config& cfg) {
    os << "\nPhysical Memory (RAM)\n\n";
    os << std::left << std::setw(17) << "Total"
       << std::setw(16) << "Used"
       << std::setw(16) << "System Cache"
       << std::setw(16) << "Available"
       << "PCT Used\n";

    os << std::left << std::setw(17) << FormatBytes(s.totalPhysical, cfg.unit)
       << std::setw(16) << FormatBytes(s.usedPhysical, cfg.unit)
       << std::setw(16) << FormatBytes(s.systemCache, cfg.unit)
       << std::setw(16) << FormatBytes(s.availPhysical, cfg.unit)
       << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "%\n\n";

    os << "\nCommit Charge (Virtual Swap)\n\n";
    os << std::left << std::setw(17) << "Limit"
       << std::setw(16) << "Committed"
       << std::setw(16) << "Peak Committed"
       << std::setw(16) << "Available"
       << "PCT Used\n";

    uint64_t commitAvail = (s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0;
    os << std::left << std::setw(17) << FormatBytes(s.commitLimit, cfg.unit)
       << std::setw(16) << FormatBytes(s.commitTotal, cfg.unit)
       << std::setw(16) << FormatBytes(s.commitPeak, cfg.unit)
       << std::setw(16) << FormatBytes(commitAvail, cfg.unit)
       << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "%\n\n";

    if (cfg.showKernelDetails) {
        os << "\nKernel Allocations\n\n";
        os << std::left << std::setw(25) << "Paged Pool:" << FormatBytes(s.kernelPaged, cfg.unit) << "\n"
           << std::left << std::setw(25) << "Non-Paged Pool:" << FormatBytes(s.kernelNonPaged, cfg.unit) << "\n\n";
    }

    if (cfg.showPagefileDetails) {
        os << "\nPaging File Topology\n\n";
        os << std::left << std::setw(32) << "DEVICE/PATH"
           << std::setw(16) << "TOTAL"
           << std::setw(16) << "USED"
           << std::setw(16) << "FREE"
           << "PCT USED\n";

        uint64_t totalPF = 0, usedPF = 0;
        for (const auto& pf : s.pageFiles) {
            totalPF += pf.totalBytes;
            usedPF  += pf.usedBytes;
            uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;

            os << std::left << std::setw(32) << Narrow(pf.path)
               << std::setw(16) << FormatBytes(pf.totalBytes, cfg.unit)
               << std::setw(16) << FormatBytes(pf.usedBytes, cfg.unit)
               << std::setw(16) << FormatBytes(freeBytes, cfg.unit)
               << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "%\n";
        }
        os << "---------------------------------------------------------------------------------------\n";
        uint64_t totalFree = (totalPF > usedPF) ? (totalPF - usedPF) : 0;
        os << std::left << std::setw(32) << "Total Pagefile Capacity"
           << std::setw(16) << FormatBytes(totalPF, cfg.unit)
           << std::setw(16) << FormatBytes(usedPF, cfg.unit)
           << std::setw(16) << FormatBytes(totalFree, cfg.unit)
           << std::fixed << std::setprecision(2) << Percent(usedPF, totalPF) << "%\n\n";
    }
}

// --- JSON FORMATTER ---
void JsonFormatter::Render(std::ostream& os, const MemorySnapshot& s, const Config&) {
    os << "{\n"
       << "  \"physical_memory\": {\n"
       << "    \"total_bytes\": " << s.totalPhysical << ",\n"
       << "    \"used_bytes\": " << s.usedPhysical << ",\n"
       << "    \"system_cache_bytes\": " << s.systemCache << ",\n"
       << "    \"available_bytes\": " << s.availPhysical << ",\n"
       << "    \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "\n"
       << "  },\n"
       << "  \"commit_charge\": {\n"
       << "    \"limit_bytes\": " << s.commitLimit << ",\n"
       << "    \"committed_bytes\": " << s.commitTotal << ",\n"
       << "    \"peak_bytes\": " << s.commitPeak << ",\n"
       << "    \"available_bytes\": " << ((s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0) << ",\n"
       << "    \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "\n"
       << "  },\n"
       << "  \"kernel_memory\": {\n"
       << "    \"paged_pool_bytes\": " << s.kernelPaged << ",\n"
       << "    \"non_paged_pool_bytes\": " << s.kernelNonPaged << "\n"
       << "  },\n"
       << "  \"paging_devices\": [\n";

    for (size_t i = 0; i < s.pageFiles.size(); ++i) {
        const auto& pf = s.pageFiles[i];
        uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;
        std::string escapedPath;
        for (char c : Narrow(pf.path)) {
            if (c == '\\') escapedPath += "\\\\";
            else escapedPath += c;
        }

        os << "    {\n"
           << "      \"path\": \"" << escapedPath << "\",\n"
           << "      \"total_bytes\": " << pf.totalBytes << ",\n"
           << "      \"used_bytes\": " << pf.usedBytes << ",\n"
           << "      \"free_bytes\": " << freeBytes << ",\n"
           << "      \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "\n"
           << "    }" << (i + 1 < s.pageFiles.size() ? "," : "") << "\n";
    }
    os << "  ]\n}\n";
}

// --- CSV FORMATTER ---
void CsvFormatter::Render(std::ostream& os, const MemorySnapshot& s, const Config&) {
    os << "Category,SubItem,TotalBytes,UsedBytes,FreeOrAvailBytes,PercentUsed\n";
    os << "RAM,Physical," << s.totalPhysical << "," << s.usedPhysical << "," << s.availPhysical << ","
       << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "\n";
    os << "RAM,SystemCache," << s.systemCache << "," << s.systemCache << ",0,100.00\n";
    os << "Commit,VirtualLimit," << s.commitLimit << "," << s.commitTotal << "," 
       << ((s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0) << ","
       << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "\n";
    os << "Kernel,Paged," << s.kernelPaged << "," << s.kernelPaged << ",0,100.00\n";
    os << "Kernel,NonPaged," << s.kernelNonPaged << "," << s.kernelNonPaged << ",0,100.00\n";

    for (const auto& pf : s.pageFiles) {
        uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;
        os << "Pagefile,\"" << Narrow(pf.path) << "\"," << pf.totalBytes << "," << pf.usedBytes << "," << freeBytes << ","
           << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "\n";
    }
}
