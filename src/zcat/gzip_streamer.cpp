#include "command_line_formatter.hpp"
#include "gzip_streamer.hpp"
#include "output_formatter.hpp"
#include "process_runner.hpp"

GzipStreamer::GzipStreamer(const ProcessRunner& runner) : m_runner(runner) {}

int GzipStreamer::EmitGzipFile(const fs::path& inputPath, OutputFormat format) const {
        std::wstring inEsc = CommandLineFormatter::EscapeSingleQuoted(inputPath.wstring());
        std::wstring outputPath;
        if (format != OutputFormat::Human) {
            wchar_t tempDir[MAX_PATH] = {}, tempFile[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            GetTempFileNameW(tempDir, L"zca", 0, tempFile);
            outputPath = tempFile;
        }

        std::wstring script =
            L"$in='" + inEsc + L"';"
            L"$fi=[System.IO.File]::OpenRead($in);"
            L"try{"
            L"$gz=New-Object System.IO.Compression.GzipStream($fi,[System.IO.Compression.CompressionMode]::Decompress);"
            + (format == OutputFormat::Human ? L"try{$gz.CopyTo([Console]::OpenStandardOutput())}finally{$gz.Dispose()}" : L"try{$fo=[IO.File]::Create('" + CommandLineFormatter::EscapeSingleQuoted(outputPath) + L"');try{$gz.CopyTo($fo)}finally{$fo.Dispose()};}finally{$gz.Dispose()}")
            + L"}finally{$fi.Dispose()}";

        int rc = m_runner.RunPowerShell(script);
        if (rc == 0 && format != OutputFormat::Human) {
            std::ifstream file(outputPath, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            DeleteFileW(outputPath.c_str());
            OutputFormatter::EmitData(data, format, std::cout);
        }
        return rc;
    }
