#include "archive_entry.hpp"
#include "archive_extract.hpp"
#include "powershell_quoting.hpp"
#include "process_capture.hpp"
#include "temporary_file.hpp"
#include "unzip_options.hpp"

int extractArchive(const UnzipOptions& opts, const fs::path& zipPath, const std::vector<ArchiveEntry>& entries) {
    if (!opts.quiet) {
        std::cout << "Archive:  " << opts.zip_filename << "\n";
    }

    fs::path destDir = fs::absolute(opts.dest_dir);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        std::cerr << "unzip error: Could not create destination directory: " << opts.dest_dir << "\n";
        return 1;
    }

    bool overwrite_all = opts.overwrite;
    bool skip_all = opts.never_overwrite;

    std::vector<std::string> selectedNames;
    selectedNames.reserve(entries.size());
    for (const auto& e : entries) {
        selectedNames.push_back(e.name);
    }

    std::string namesArray = "@(";
    for (size_t i = 0; i < selectedNames.size(); ++i) {
        if (i != 0) namesArray += ",";
        namesArray += ps_quote(selectedNames[i]);
    }
    namesArray += ")";

    std::string psExtractCmd =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
        "$sel=@{}; foreach($n in " + namesArray + "){ $sel[$n]=$true }; "
        "$out=@(); "
        "foreach($e in $zip.Entries){ "
        "  if($e.FullName.EndsWith('/')){continue}; "
        "  if(-not $sel.ContainsKey($e.FullName)){continue}; "
        "  $out += $e.FullName "
        "}; "
        "$out | ForEach-Object { Write-Output $_ }; "
        "$zip.Dispose()\"";

    int selExit = 0;
    std::string selOut = run_capture(psExtractCmd, selExit);
    if (selExit != 0) {
        std::cerr << "unzip error: Failed to enumerate selected entries from archive.\n";
        return 1;
    }

    std::vector<std::string> entriesToProcess;
    {
        std::istringstream iss(selOut);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) entriesToProcess.push_back(line);
        }
    }

    for (const auto& rawName : entriesToProcess) {
        fs::path rel(rawName);
        fs::path entryName = opts.junk_paths ? rel.filename() : rel;
        fs::path targetPath = destDir / entryName;
        fs::path targetNorm = fs::weakly_canonical(targetPath.parent_path(), ec) / targetPath.filename();

        fs::path destNorm = fs::weakly_canonical(destDir, ec);
        auto mismatchPair = std::mismatch(destNorm.begin(), destNorm.end(), targetNorm.begin());
        if (mismatchPair.first != destNorm.end()) {
            std::cerr << "unzip warning: Skipping insecure path: " << rawName << "\n";
            continue;
        }

        fs::create_directories(targetPath.parent_path(), ec);

        if (fs::exists(targetPath)) {
            if (skip_all) {
                continue;
            }

            if (!overwrite_all) {
                std::cout << "replace " << targetPath.string() << "? [y]es, [n]o, [A]ll, [N]one: ";
                std::string resp;
                std::cin >> resp;

                if (resp == "y" || resp == "Y") {
                } else if (resp == "n" || resp == "N") {
                    continue;
                } else if (resp == "A" || resp == "a") {
                    overwrite_all = true;
                } else if (resp == "N" || resp == "none") {
                    skip_all = true;
                    continue;
                } else {
                    continue;
                }
            }
        }

        if (!opts.quiet) {
            std::cout << "  inflating: " << targetPath.string() << "\n";
        }

        std::string tmpExtractDir = make_temp_file("cmdext-unzip-stage");
        fs::remove(tmpExtractDir, ec);
        fs::create_directories(tmpExtractDir, ec);

        std::string extractOne =
            "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
            "$ErrorActionPreference='Stop'; "
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
            "$entry=$zip.GetEntry(" + ps_quote(rawName) + "); "
            "if(-not $entry){ throw 'entry not found' }; "
            "$tmp=" + ps_quote(tmpExtractDir) + "; "
            "$target=Join-Path $tmp $entry.Name; "
            "[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true); "
            "$zip.Dispose()\"";

        int oneExit = 0;
        std::string oneOut = run_capture(extractOne, oneExit);
        (void)oneOut;
        if (oneExit != 0) {
            std::cerr << "unzip error: Failed to extract " << rawName << "\n";
            fs::remove_all(tmpExtractDir, ec);
            continue;
        }

        fs::path stagedFile = fs::path(tmpExtractDir) / fs::path(rawName).filename();
        if (!fs::exists(stagedFile)) {
            std::cerr << "unzip error: Missing staged file for " << rawName << "\n";
            fs::remove_all(tmpExtractDir, ec);
            continue;
        }

        std::error_code copyEc;
        fs::copy_file(stagedFile, targetPath, fs::copy_options::overwrite_existing, copyEc);
        fs::remove_all(tmpExtractDir, ec);

        if (copyEc) {
            std::cerr << "unzip error: Failed to write " << targetPath.string() << "\n";
        }
    }

    return 0;
}

