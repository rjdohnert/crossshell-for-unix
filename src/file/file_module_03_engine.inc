class MagicSignatureDetector {
private:
    static bool startsWith(const std::vector<unsigned char>& data, const std::vector<unsigned char>& sig) {
        if (data.size() < sig.size()) return false;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (data[i] != sig[i]) return false;
        }
        return true;
    }

    static bool isTextContent(const std::vector<unsigned char>& data) {
        if (data.empty()) return true;
        size_t printable = 0;
        for (unsigned char c : data) {
            if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c <= 126)) {
                ++printable;
            }
        }
        return printable * 100 / data.size() > 90;
    }

public:
    static std::wstring detectType(const std::wstring& path, bool mime) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return mime ? L"cannot-open/not-found" : L"cannot open (No such file or directory)";
        }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            return mime ? L"inode/directory" : L"directory";
        }
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            return mime ? L"inode/symlink" : L"symbolic link or reparse point";
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return mime ? L"cannot-open/permission-denied" : L"cannot open";
        }

        std::vector<unsigned char> data(4096);
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        data.resize(static_cast<size_t>(in.gcount()));

        if (data.empty()) return mime ? L"application/x-empty" : L"empty";

        if (startsWith(data, {0x4D, 0x5A})) return mime ? L"application/x-dosexec" : L"PE executable (MZ)";
        if (startsWith(data, {0x7F, 0x45, 0x4C, 0x46})) return mime ? L"application/x-executable" : L"ELF executable";
        if (startsWith(data, {0x50, 0x4B, 0x03, 0x04})) return mime ? L"application/zip" : L"ZIP archive";
        if (startsWith(data, {0x1F, 0x8B})) return mime ? L"application/gzip" : L"gzip compressed data";
        if (startsWith(data, {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00})) return mime ? L"application/x-xz" : L"XZ compressed data";
        if (startsWith(data, {0x89, 0x50, 0x4E, 0x47})) return mime ? L"image/png" : L"PNG image";
        if (startsWith(data, {0xFF, 0xD8, 0xFF})) return mime ? L"image/jpeg" : L"JPEG image";
        if (startsWith(data, {0x47, 0x49, 0x46, 0x38})) return mime ? L"image/gif" : L"GIF image";
        if (startsWith(data, {0x25, 0x50, 0x44, 0x46})) return mime ? L"application/pdf" : L"PDF document";
        if (startsWith(data, {0x42, 0x4D})) return mime ? L"image/bmp" : L"BMP image";

        if (isTextContent(data)) {
            return mime ? L"text/plain; charset=us-ascii" : L"ASCII text";
        }

        return mime ? L"application/octet-stream" : L"data";
    }
};

class FileEngine {
private:
    FileOptions options;

public:
    explicit FileEngine(FileOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::pair<std::wstring, std::wstring>> results;

        for (const auto& path : options.files) {
            std::wstring type = MagicSignatureDetector::detectType(path, options.mime);
            results.emplace_back(path, type);
        }

        return FileReporter::dispatch(results, options.outputFormat, options.brief, options.pipeCommand);
    }
};
