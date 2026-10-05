// ============================================================================
// 3. CHARSET CONVERSION ENGINE
// ============================================================================

class CharsetConverter {
public:
    static bool convert(const std::string& input, UINT fromCp, UINT toCp, std::string& output, bool discardInvalid) {
        if (fromCp == toCp) {
            output = input;
            return true;
        }

        // Convert FromCP -> UTF-16 wstring
        int wLen = MultiByteToWideChar(fromCp, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
        if (wLen <= 0) {
            if (discardInvalid) { output = input; return true; }
            return false;
        }

        std::wstring wide(wLen, L'\0');
        MultiByteToWideChar(fromCp, 0, input.data(), static_cast<int>(input.size()), &wide[0], wLen);

        // Convert UTF-16 wstring -> ToCP
        int outLen = WideCharToMultiByte(toCp, 0, wide.data(), wLen, nullptr, 0, nullptr, nullptr);
        if (outLen <= 0) {
            if (discardInvalid) return true;
            return false;
        }

        output.resize(outLen);
        WideCharToMultiByte(toCp, 0, wide.data(), wLen, &output[0], outLen, nullptr, nullptr);
        return true;
    }
};

class IconvEngine {
private:
    IconvOptions options;

public:
    explicit IconvEngine(IconvOptions opts) : options(std::move(opts)) {}

    int execute() {
        if (options.list) {
            EncodingRegistry::listSupported();
            return 0;
        }

        UINT fromCp = EncodingRegistry::lookupCodePage(options.fromCode.empty() ? "utf8" : options.fromCode);
        UINT toCp = EncodingRegistry::lookupCodePage(options.toCode.empty() ? "utf8" : options.toCode);

        if (fromCp == 0) {
            std::cerr << "iconv: conversion from " << options.fromCode << " is not supported\n";
            return 1;
        }
        if (toCp == 0) {
            std::cerr << "iconv: conversion to " << options.toCode << " is not supported\n";
            return 1;
        }

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        std::ostream* out = &std::cout;
        std::ofstream fileOut;
        if (!options.outputFile.empty()) {
            fileOut.open(options.outputFile, std::ios::binary);
            if (!fileOut.is_open()) {
                std::cerr << "iconv: cannot open '" << options.outputFile << "' for writing\n";
                return 1;
            }
            out = &fileOut;
        }

        for (const auto& path : options.inputFiles) {
            std::istream* in = &std::cin;
            std::ifstream fileIn;
            if (path != "-") {
                fileIn.open(path, std::ios::binary);
                if (!fileIn.is_open()) {
                    if (!options.silent) std::cerr << "iconv: cannot open '" << path << "'\n";
                    return 1;
                }
                in = &fileIn;
            }

            std::string content((std::istreambuf_iterator<char>(*in)), std::istreambuf_iterator<char>());
            std::string converted;

            if (!CharsetConverter::convert(content, fromCp, toCp, converted, options.discardInvalid)) {
                if (!options.silent) std::cerr << "iconv: conversion failed on " << path << "\n";
                return 1;
            }

            IconvReporter::dispatch(converted, options.outputFormat, options.pipeCommand, *out);
        }

        return 0;
    }
};

