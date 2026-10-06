#include "archive_filter.hpp"
#include "archive_test.hpp"
#include "powershell_quoting.hpp"
#include "process_capture.hpp"
#include "unzip_options.hpp"

int testArchive(const UnzipOptions& opts, const fs::path& zipPath) {
        if (!opts.quiet) std::cout << "Archive:  " << opts.zip_filename << "\n";

        std::string testCmd =
            "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
            "$ErrorActionPreference='Stop'; "
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
            "$ok=$true; "
            "foreach($e in $zip.Entries){ "
            "  if($e.FullName.EndsWith('/')){continue}; "
            "  $s=$null; "
            "  try{ $s=$e.Open(); $buf=New-Object byte[] 65536; while(($n=$s.Read($buf,0,$buf.Length)) -gt 0){}; "
            "       Write-Output ('OK\t' + $e.FullName) }"
            "  catch{ Write-Output ('FAIL\t' + $e.FullName); $ok=$false }"
            "  finally{ if($s){$s.Dispose()} }"
            "}; "
            "$zip.Dispose(); if(-not $ok){exit 1}\"";

        int testExit = 0;
        std::string testOut = run_capture(testCmd, testExit);

        std::istringstream iss(testOut);
        std::string line;
        bool all_ok = (testExit == 0);
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;

            std::string status = line.substr(0, tab);
            std::string name = line.substr(tab + 1);
            if (!should_extract(name, opts.filters)) continue;

            if (!opts.quiet) {
                if (status == "OK") {
                    std::cout << "    testing: " << name << "   OK\n";
                } else {
                    std::cout << "    testing: " << name << "   FAILED\n";
                }
            }

            if (status != "OK") all_ok = false;
        }

        if (!all_ok) {
            std::cerr << "Errors detected in " << opts.zip_filename << "!\n";
            return 1;
        }

        if (!opts.quiet) {
            std::cout << "No errors detected in compressed data of " << opts.zip_filename << ".\n";
        }
        return 0;
    }
