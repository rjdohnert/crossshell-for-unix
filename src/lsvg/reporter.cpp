#include "reporter.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

std::string LsvgReporter::CsvEscape(const std::string& value) {
    std::string out = "\"";
    for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
    return out + '"';
}

std::string LsvgReporter::JsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

void LsvgReporter::PrintVGDetail(const VolumeGroup& vg) {
    UINT32 totalPPs = static_cast<UINT32>(vg.totalSizeBytes / PP_SIZE_BYTES);
    UINT32 freePPs = static_cast<UINT32>(vg.freeSizeBytes / PP_SIZE_BYTES);
    UINT32 usedPPs = totalPPs - freePPs;
    UINT32 openLVs = 0;
    for (const auto& lv : vg.lvs) {
        if (lv.state.find("open") != std::string::npos) openLVs++;
    }

    std::cout << std::left
              << std::setw(20) << "VOLUME GROUP:" << std::setw(25) << vg.name
              << std::setw(16) << "VG IDENTIFIER:" << vg.vgIdentifier << "\n"
              << std::setw(20) << "VG STATE:" << std::setw(25) << vg.state
              << std::setw(16) << "PP SIZE:" << vg.ppSizeMB << " megabyte(s)\n"
              << std::setw(20) << "VG PERMISSION:" << std::setw(25) << vg.permission
              << std::setw(16) << "TOTAL PPs:" << totalPPs << " (" << (totalPPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "MAX LVs:" << std::setw(25) << vg.maxLVs
              << std::setw(16) << "FREE PPs:" << freePPs << " (" << (freePPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "LVs:" << std::setw(25) << vg.lvs.size()
              << std::setw(16) << "USED PPs:" << usedPPs << " (" << (usedPPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "OPEN LVs:" << std::setw(25) << openLVs
              << std::setw(16) << "QUORUM:" << vg.quorum << "\n"
              << std::setw(20) << "TOTAL PVs:" << std::setw(25) << vg.pvs.size()
              << std::setw(16) << "ACTIVE PVs:" << vg.pvs.size() << "\n"
              << std::setw(20) << "STALE PVs:" << std::setw(25) << 0
              << std::setw(16) << "MAX PPs per PV:" << 1016 << "\n\n";
}

void LsvgReporter::PrintLVDetail(const VolumeGroup& vg) {
    std::cout << vg.name << ":\n";
    std::cout << std::left
              << std::setw(18) << "LV NAME"
              << std::setw(12) << "TYPE"
              << std::setw(8)  << "LPs"
              << std::setw(8)  << "PPs"
              << std::setw(6)  << "PVs"
              << std::setw(14) << "LV STATE"
              << "MOUNT POINT\n";
    std::cout << std::string(82, '-') << "\n";

    for (const auto& lv : vg.lvs) {
        std::cout << std::left
                  << std::setw(18) << lv.name
                  << std::setw(12) << lv.type
                  << std::setw(8)  << lv.lpCount
                  << std::setw(8)  << lv.ppCount
                  << std::setw(6)  << lv.pvCount
                  << std::setw(14) << lv.state
                  << lv.mountPoint << "\n";
    }
    std::cout << "\n";
}

void LsvgReporter::PrintPVDetail(const VolumeGroup& vg) {
    std::cout << vg.name << ":\n";
    std::cout << std::left
              << std::setw(18) << "PV_NAME"
              << std::setw(16) << "PV STATE"
              << std::setw(12) << "TOTAL PPs"
              << std::setw(12) << "FREE PPs"
              << "FREE DISTRIBUTION\n";
    std::cout << std::string(82, '-') << "\n";

    for (const auto& pv : vg.pvs) {
        std::ostringstream dist;
        dist << (pv.freePPs / 5) << ".." << (pv.freePPs / 5) << ".." 
             << (pv.freePPs / 5) << ".." << (pv.freePPs / 5) << ".." 
             << (pv.freePPs - (4 * (pv.freePPs / 5)));

        std::cout << std::left
                  << std::setw(18) << pv.name
                  << std::setw(16) << pv.state
                  << std::setw(12) << pv.totalPPs
                  << std::setw(12) << pv.freePPs
                  << dist.str() << "\n";
    }
    std::cout << "\n";
}

void LsvgReporter::ReportFormatted(const std::vector<VolumeGroup>& vgs, const CmdOptions& opts) {
    std::vector<VolumeGroup> selected;
    for (const auto& vg : vgs) {
        bool keep = opts.targetVG.empty();
        if (!opts.targetVG.empty()) keep = _stricmp(vg.name.c_str(), opts.targetVG.c_str()) == 0;
        if (keep) selected.push_back(vg);
    }
    if (opts.format == LsvgFormat::Csv) {
        std::cout << "Name,Identifier,State,Permission,TotalBytes,FreeBytes,LogicalVolumes,PhysicalVolumes\n";
        for (const auto& vg : selected) {
            std::cout << CsvEscape(vg.name) << ',' << CsvEscape(vg.vgIdentifier) << ',' << CsvEscape(vg.state) << ','
                      << CsvEscape(vg.permission) << ',' << vg.totalSizeBytes << ',' << vg.freeSizeBytes << ','
                      << vg.lvs.size() << ',' << vg.pvs.size() << '\n';
        }
    } else {
        std::cout << "[\n";
        for (size_t i = 0; i < selected.size(); ++i) {
            const auto& vg = selected[i];
            std::cout << "  {\"name\":\"" << JsonEscape(vg.name)
                      << "\",\"identifier\":\"" << JsonEscape(vg.vgIdentifier)
                      << "\",\"state\":\"" << JsonEscape(vg.state)
                      << "\",\"permission\":\"" << JsonEscape(vg.permission)
                      << "\",\"totalBytes\":" << vg.totalSizeBytes
                      << ",\"freeBytes\":" << vg.freeSizeBytes
                      << ",\"logicalVolumes\":" << vg.lvs.size()
                      << ",\"physicalVolumes\":" << vg.pvs.size() << "}"
                      << (i + 1 == selected.size() ? "\n" : ",\n");
        }
        std::cout << "]\n";
    }
}
