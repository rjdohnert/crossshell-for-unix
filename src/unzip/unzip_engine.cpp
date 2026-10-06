#include "archive_entry.hpp"
#include "archive_extract.hpp"
#include "archive_filter.hpp"
#include "archive_list.hpp"
#include "archive_test.hpp"
#include "output_session.hpp"
#include "powershell_quoting.hpp"
#include "process_capture.hpp"
#include "temporary_file.hpp"
#include "unzip_engine.hpp"
#include "unzip_options.hpp"

int executeUnzip(const UnzipOptions& opts) {
    OutputSession output_session(opts.output_format, opts.pipe_command);

    fs::path zipPath = fs::absolute(opts.zip_filename);
    if (!fs::exists(zipPath)) {
        std::cerr << "unzip error: Cannot open " << opts.zip_filename << "\n";
        return 1;
    }

    std::string listFile = make_temp_file("cmdext-unzip-list");
    std::string psListCmd =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
        "foreach($e in $zip.Entries){ "
        "  if($e.FullName.EndsWith('/')){continue}; "
        "  $dt=$e.LastWriteTime.UtcDateTime.ToString('yyyy-MM-dd HH:mm'); "
        "  Write-Output ($e.Length.ToString() + '\t' + $dt + '\t' + $e.FullName)"
        "}; "
        "$zip.Dispose()\"";

    int psExit = 0;
    std::string listOutput = run_capture(psListCmd, psExit);
    if (psExit != 0) {
        std::cerr << "unzip error: Failed to read zip archive metadata.\n";
        return 1;
    }

    {
        std::ofstream ofs(listFile, std::ios::binary);
        ofs << listOutput;
    }

    std::vector<ArchiveEntry> entries;
    {
        std::ifstream ifs(listFile, std::ios::binary);
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t t1 = line.find('\t');
            size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
            if (t1 == std::string::npos || t2 == std::string::npos) continue;

            ArchiveEntry e;
            try {
                e.size = static_cast<uint64_t>(std::stoull(line.substr(0, t1)));
            } catch (...) {
                continue;
            }
            e.date = line.substr(t1 + 1, t2 - (t1 + 1));
            e.name = line.substr(t2 + 1);

            if (should_extract(e.name, opts.filters)) {
                entries.push_back(std::move(e));
            }
        }
    }

    std::error_code ecRemove;
    fs::remove(listFile, ecRemove);

    if (opts.list) return listArchive(opts, entries);
    if (opts.test) return testArchive(opts, zipPath);
    return extractArchive(opts, zipPath, entries);
}
