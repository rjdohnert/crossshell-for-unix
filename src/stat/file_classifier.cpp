#include "file_classifier.hpp"

FileClassifier::AnalysisResult FileClassifier::analyze(const std::wstring& path, uint64_t fileSize) {
        AnalysisResult result;

        if (fileSize == 0) {
            result.typeDescription = L"Empty File";
            result.isText = true;
            result.lineCount = 0;
            return result;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            result.typeDescription = L"Unreadable File";
            return result;
        }

        // Read magic header bytes
        const size_t sampleSize = 4096;
        std::vector<char> buffer(sampleSize);
        file.read(buffer.data(), sampleSize);
        std::streamsize bytesRead = file.gcount();

        // 1. Identify Magic Numbers / Known Signatures
        std::wstring magicType = identifyMagicBytes(reinterpret_cast<const uint8_t*>(buffer.data()), bytesRead);
        if (!magicType.empty()) {
            result.typeDescription = magicType;
            result.isText = false;
            result.lineCount = -1;
            return result;
        }

        // 2. Binary vs Text Heuristic (Check for NULL bytes or high control codes)
        bool isBinary = false;
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            uint8_t ch = static_cast<uint8_t>(buffer[i]);
            if (ch == 0 || (ch < 7 || (ch > 14 && ch < 32 && ch != 27))) {
                isBinary = true;
                break;
            }
        }

        if (isBinary) {
            result.typeDescription = L"Binary Data";
            result.isText = false;
            result.lineCount = -1;
            return result;
        }

        // 3. Text file categorization by extension
        fs::path p(path);
        std::wstring ext = p.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        result.typeDescription = classifyTextExtension(ext);
        result.isText = true;

        // 4. Count lines across the entire file
        file.clear();
        file.seekg(0, std::ios::beg);

        int64_t lines = 0;
        char chunk[65536];
        bool hasChars = false;
        char lastChar = '\0';

        while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) {
            std::streamsize n = file.gcount();
            hasChars = true;
            for (std::streamsize i = 0; i < n; ++i) {
                if (chunk[i] == '\n') {
                    lines++;
                }
                lastChar = chunk[i];
            }
        }

        // If file has content and does not end with \n, count trailing line
        if (hasChars && lastChar != '\n') {
            lines++;
        }

        result.lineCount = lines;
        return result;
    }

std::wstring FileClassifier::identifyMagicBytes(const uint8_t* buf, std::streamsize len) {
        if (len >= 2 && buf[0] == 'M' && buf[1] == 'Z') return L"Executable (Win32 PE/DLL)";
        if (len >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') return L"ELF Binary";
        if (len >= 4 && buf[0] == 'P' && buf[1] == 'K' && buf[2] == 0x03 && buf[3] == 0x04) return L"ZIP Archive / Office OpenXML";
        if (len >= 4 && buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' && buf[3] == 'F') return L"PDF Document";
        if (len >= 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') return L"PNG Image";
        if (len >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) return L"JPEG Image";
        if (len >= 6 && (memcmp(buf, "GIF87a", 6) == 0 || memcmp(buf, "GIF89a", 6) == 0)) return L"GIF Image";
        if (len >= 6 && memcmp(buf, "7z\xBC\xAF\x27\x1C", 6) == 0) return L"7-Zip Archive";
        if (len >= 7 && memcmp(buf, "Rar!\x1A\x07\x00", 7) == 0) return L"RAR Archive";
        if (len >= 2 && buf[0] == 0x1F && buf[1] == 0x8B) return L"GZIP Compressed File";
        if (len >= 4 && (memcmp(buf, "RIFF", 4) == 0)) return L"RIFF Container (WAV/AVI/WEBP)";
        return L"";
    }

std::wstring FileClassifier::classifyTextExtension(const std::wstring& ext) {
        if (ext == L".cpp" || ext == L".cxx" || ext == L".cc" || ext == L".h" || ext == L".hpp") return L"C/C++ Source";
        if (ext == L".cs") return L"C# Source";
        if (ext == L".rs") return L"Rust Source";
        if (ext == L".py") return L"Python Script";
        if (ext == L".js" || ext == L".mjs" || ext == L".ts") return L"JavaScript/TypeScript";
        if (ext == L".json") return L"JSON Document";
        if (ext == L".xml" || ext == L".xaml") return L"XML Document";
        if (ext == L".html" || ext == L".htm") return L"HTML Document";
        if (ext == L".css" || ext == L".scss") return L"Cascading Style Sheet";
        if (ext == L".md" || ext == L".markdown") return L"Markdown Document";
        if (ext == L".txt" || ext == L".log") return L"Plain Text Document";
        if (ext == L".ps1" || ext == L".bat" || ext == L".cmd") return L"Windows Script/Batch";
        if (ext == L".sh" || ext == L".bash") return L"Shell Script";
        if (ext == L".sql") return L"SQL Database Script";
        if (ext == L".csv") return L"CSV Data File";
        if (ext == L".yaml" || ext == L".yml") return L"YAML Document";
        return L"Text Document";
    }
