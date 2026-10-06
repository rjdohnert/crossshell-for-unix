#include "command_line_formatter.hpp"
#include "gzip_decompressor.hpp"
#include "process_runner.hpp"

GzipDecompressor::GzipDecompressor(const ProcessRunner& runner) : m_runner(runner) {}

bool GzipDecompressor::Decompress(const fs::path& inputPath, const fs::path& outputPath) const {
        std::wstring inEsc = CommandLineFormatter::EscapeSingleQuoted(inputPath.wstring());
        std::wstring outEsc = CommandLineFormatter::EscapeSingleQuoted(outputPath.wstring());

        std::wstring script =
            L"$in='" + inEsc + L"';"
            L"$out='" + outEsc + L"';"
            L"$fi=[System.IO.File]::OpenRead($in);"
            L"try{"
            L"$gz=New-Object System.IO.Compression.GzipStream($fi,[System.IO.Compression.CompressionMode]::Decompress);"
            L"try{"
            L"$fo=[System.IO.File]::Create($out);"
            L"try{$gz.CopyTo($fo)}finally{$fo.Dispose()}"
            L"}finally{$gz.Dispose()}"
            L"}finally{$fi.Dispose()}";

        std::vector<std::wstring> psArgs = {
            L"powershell",
            L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-Command",
            script
        };

        return m_runner.Run(nullptr, psArgs) == 0;
    }
