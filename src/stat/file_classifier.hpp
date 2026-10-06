#pragma once

#include "stat.hpp"

class FileClassifier {
public:
    struct AnalysisResult {
        std::wstring typeDescription;
        bool isText = false;
        int64_t lineCount = -1;
    };

    static AnalysisResult analyze(const std::wstring& path, uint64_t fileSize);

private:
    static std::wstring identifyMagicBytes(const uint8_t* buf, std::streamsize len);

    static std::wstring classifyTextExtension(const std::wstring& ext);
};
