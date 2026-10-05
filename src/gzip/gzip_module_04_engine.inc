class GzipEngine {
private:
    GzipOptions options;

public:
    explicit GzipEngine(GzipOptions opts) : options(std::move(opts)) {}

    int execute() {
        bool allOk = true;
        std::vector<std::pair<std::wstring, std::wstring>> ops;

        for (const auto& file : options.files) {
            std::wstring src = file;
            DWORD srcAttributes = GetFileAttributesW(src.c_str());

            if (srcAttributes == INVALID_FILE_ATTRIBUTES || (srcAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                std::wcerr << L"gzip: " << file << L": No such file\n";
                allOk = false;
                continue;
            }

            if (options.decompress) {
                std::wstring dst;
                if (src.size() > 3 && src.rfind(L".gz") == src.size() - 3) {
                    dst = src.substr(0, src.size() - 3);
                } else {
                    dst = src + L".out";
                }

                if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES && !options.force) {
                    std::wcerr << L"gzip: " << dst << L" already exists; use -f to overwrite\n";
                    allOk = false;
                    continue;
                }

                if (PowerShellGzipBridge::decompressFile(src, dst)) {
                    if (!options.keep) DeleteFileW(src.c_str());
                    ops.emplace_back(src, dst);
                } else {
                    std::wcerr << L"gzip: error decompressing " << file << L"\n";
                    allOk = false;
                }
            } else {
                std::wstring dst = src + L".gz";
                if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES && !options.force) {
                    std::wcerr << L"gzip: " << dst << L" already exists; use -f to overwrite\n";
                    allOk = false;
                    continue;
                }

                if (PowerShellGzipBridge::compressFile(src, dst)) {
                    if (!options.keep) DeleteFileW(src.c_str());
                    ops.emplace_back(src, dst);
                } else {
                    std::wcerr << L"gzip: error compressing " << file << L"\n";
                    allOk = false;
                }
            }
        }

        GzipReporter::dispatch(ops, options.outputFormat, options.pipeCommand);
        return allOk ? 0 : 1;
    }
};

