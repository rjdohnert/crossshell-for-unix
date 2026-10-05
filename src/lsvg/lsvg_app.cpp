#include "lsvg_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

int LsvgApplication::Run(int argc, char* argv[]) {
    CmdOptions opts;
    if (!opts.Parse(argc, argv)) {
        return 0; // Help or version already printed
    }

    auto vgs = StorageManager::DiscoverVolumeGroups();
    if (!opts.filters.empty()) opts.targetVG = opts.filters.front();

    if (opts.format != LsvgFormat::Table) {
        std::vector<VolumeGroup> selected;
        for (const auto& vg : vgs) {
            bool keep = opts.targetVG.empty();
            if (!opts.targetVG.empty()) keep = _stricmp(vg.name.c_str(), opts.targetVG.c_str()) == 0;
            if (keep) selected.push_back(vg);
        }
        LsvgReporter::ReportFormatted(vgs, opts);
        return selected.empty() ? 1 : 0;
    }

    // Mode 1: Plain 'lsvg' or 'lsvg -o' -> show the detailed summary for each discovered VG
    if (!opts.listAllDetail && !opts.listLVs && !opts.listPVs && opts.targetVG.empty()) {
        for (const auto& vg : vgs) {
            LsvgReporter::PrintVGDetail(vg);
        }
        return 0;
    }

    // Mode 2: 'lsvg -a' (All VG details)
    if (opts.listAllDetail) {
        for (const auto& vg : vgs) {
            LsvgReporter::PrintVGDetail(vg);
        }
        return 0;
    }

    // Mode 3: Specific VG requested
    auto it = std::find_if(vgs.begin(), vgs.end(), [&](const VolumeGroup& v) {
        return _stricmp(v.name.c_str(), opts.targetVG.c_str()) == 0;
    });

    if (it == vgs.end()) {
        std::cerr << "0516-010 lsvg: Volume group " << opts.targetVG << " does not exist or is not active.\n";
        return 1;
    }

    const auto& vg = *it;

    if (opts.listLVs) {
        LsvgReporter::PrintLVDetail(vg);
    } else if (opts.listPVs) {
        LsvgReporter::PrintPVDetail(vg);
    } else {
        LsvgReporter::PrintVGDetail(vg);
    }

    return 0;
}
