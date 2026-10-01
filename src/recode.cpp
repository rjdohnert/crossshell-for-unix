/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <optional>

namespace fs = std::filesystem;

// ============================================================================
// 1. TRANSFORMER INTERFACE & CONCRETE IMPLEMENTATIONS
// ============================================================================

class ITransformer {
public:
    virtual ~ITransformer() = default;
    [[nodiscard]] virtual std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const = 0;
    [[nodiscard]] virtual std::string getName() const = 0;
};

// --- Windows Native Code Page / Charset Transformer ---
class WindowsCodePageTransformer : public ITransformer {
private:
    uint32_t fromCodePage;
    uint32_t toCodePage;
    std::string name;

public:
    WindowsCodePageTransformer(uint32_t fromCP, uint32_t toCP, std::string transformerName)
        : fromCodePage(fromCP), toCodePage(toCP), name(std::move(transformerName)) {}

    [[nodiscard]] std::string getName() const override { return name; }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        if (input.empty()) return {};
        if (fromCodePage == toCodePage) return input;

        // 1. Source MultiByte/CodePage -> UTF-16
        int wideLen = MultiByteToWideChar(
            fromCodePage, 0,
            reinterpret_cast<const char*>(input.data()),
            static_cast<int>(input.size()),
            nullptr, 0
        );

        if (wideLen <= 0) return input;

        std::wstring wideBuffer(wideLen, L'\0');
        MultiByteToWideChar(
            fromCodePage, 0,
            reinterpret_cast<const char*>(input.data()),
            static_cast<int>(input.size()),
            wideBuffer.data(), wideLen
        );

        // 2. UTF-16 -> Destination CodePage
        int targetLen = WideCharToMultiByte(
            toCodePage, 0,
            wideBuffer.data(), wideLen,
            nullptr, 0, nullptr, nullptr
        );

        if (targetLen <= 0) return {};

        std::vector<uint8_t> output(targetLen);
        WideCharToMultiByte(
            toCodePage, 0,
            wideBuffer.data(), wideLen,
            reinterpret_cast<char*>(output.data()), targetLen,
            nullptr, nullptr
        );

        return output;
    }
};

// --- UTF-16LE / UTF-8 Converter ---
class Utf16Transformer : public ITransformer {
private:
    bool toUtf16;

public:
    explicit Utf16Transformer(bool to16) : toUtf16(to16) {}

    [[nodiscard]] std::string getName() const override {
        return toUtf16 ? "UTF-8..UTF-16LE" : "UTF-16LE..UTF-8";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        if (input.empty()) return {};

        if (toUtf16) {
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(input.data()), static_cast<int>(input.size()), nullptr, 0);
            if (wideLen <= 0) return {};
            std::vector<uint8_t> output(wideLen * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(input.data()), static_cast<int>(input.size()), reinterpret_cast<wchar_t*>(output.data()), wideLen);
            return output;
        } else {
            int wideLen = static_cast<int>(input.size() / sizeof(wchar_t));
            const auto* wideStr = reinterpret_cast<const wchar_t*>(input.data());
            int targetLen = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideLen, nullptr, 0, nullptr, nullptr);
            if (targetLen <= 0) return {};
            std::vector<uint8_t> output(targetLen);
            WideCharToMultiByte(CP_UTF8, 0, wideStr, wideLen, reinterpret_cast<char*>(output.data()), targetLen, nullptr, nullptr);
            return output;
        }
    }
};

// --- Base64 Transformer ---
class Base64Transformer : public ITransformer {
private:
    bool encode;
    static constexpr char b64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

public:
    explicit Base64Transformer(bool isEncode) : encode(isEncode) {}

    [[nodiscard]] std::string getName() const override {
        return encode ? "Encode Base64" : "Decode Base64";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        return encode ? encodeBase64(input) : decodeBase64(input);
    }

private:
    static std::vector<uint8_t> encodeBase64(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;
        int val = 0, valb = -6;
        for (uint8_t c : data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(static_cast<uint8_t>(b64Table[(val >> valb) & 0x3F]));
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(static_cast<uint8_t>(b64Table[((val << 8) >> (valb + 8)) & 0x3F]));
        while (out.size() % 4) out.push_back('=');
        return out;
    }

    static std::vector<uint8_t> decodeBase64(const std::vector<uint8_t>& data) {
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[static_cast<uint8_t>(b64Table[i])] = i;

        std::vector<uint8_t> out;
        int val = 0, valb = -8;
        for (uint8_t c : data) {
            if (T[c] == -1) continue;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
};

// --- Hexadecimal Transformer ---
class HexTransformer : public ITransformer {
private:
    bool encode;
    static constexpr char hexLookup[] = "0123456789abcdef";

public:
    explicit HexTransformer(bool isEncode) : encode(isEncode) {}

    [[nodiscard]] std::string getName() const override {
        return encode ? "Encode Hex" : "Decode Hex";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        if (encode) {
            std::vector<uint8_t> out;
            out.reserve(input.size() * 2);
            for (uint8_t b : input) {
                out.push_back(hexLookup[b >> 4]);
                out.push_back(hexLookup[b & 0x0F]);
            }
            return out;
        } else {
            std::vector<uint8_t> out;
            int high = -1;
            for (uint8_t c : input) {
                if (std::isspace(c)) continue;
                int val = -1;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                if (val < 0) continue;

                if (high == -1) {
                    high = val;
                } else {
                    out.push_back(static_cast<uint8_t>((high << 4) | val));
                    high = -1;
                }
            }
            return out;
        }
    }
};

// --- URL / Percent-Encoding Transformer ---
class UrlTransformer : public ITransformer {
private:
    bool encode;

public:
    explicit UrlTransformer(bool isEncode) : encode(isEncode) {}

    [[nodiscard]] std::string getName() const override {
        return encode ? "Encode URL" : "Decode URL";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        if (encode) {
            std::ostringstream escaped;
            for (uint8_t c : input) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << static_cast<char>(c);
                } else {
                    escaped << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                }
            }
            std::string str = escaped.str();
            return {str.begin(), str.end()};
        } else {
            std::vector<uint8_t> out;
            for (size_t i = 0; i < input.size(); ++i) {
                if (input[i] == '%' && i + 2 < input.size()) {
                    std::string hexStr = {static_cast<char>(input[i + 1]), static_cast<char>(input[i + 2])};
                    try {
                        auto val = static_cast<uint8_t>(std::stoul(hexStr, nullptr, 16));
                        out.push_back(val);
                        i += 2;
                        continue;
                    } catch (...) {}
                } else if (input[i] == '+') {
                    out.push_back(' ');
                    continue;
                }
                out.push_back(input[i]);
            }
            return out;
        }
    }
};

// --- HTML Entities Transformer ---
class HtmlEntityTransformer : public ITransformer {
private:
    bool encode;

public:
    explicit HtmlEntityTransformer(bool isEncode) : encode(isEncode) {}

    [[nodiscard]] std::string getName() const override {
        return encode ? "Encode HTML Entities" : "Decode HTML Entities";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        std::string str(input.begin(), input.end());
        if (encode) {
            std::string out;
            for (char c : str) {
                switch (c) {
                    case '&':  out.append("&amp;"); break;
                    case '\"': out.append("&quot;"); break;
                    case '\'': out.append("&#39;"); break;
                    case '<':  out.append("&lt;"); break;
                    case '>':  out.append("&gt;"); break;
                    default:   out.push_back(c); break;
                }
            }
            return {out.begin(), out.end()};
        } else {
            static const std::unordered_map<std::string, char> entities = {
                {"&amp;", '&'}, {"&quot;", '\"'}, {"&#39;", '\''}, {"&apos;", '\''},
                {"&lt;", '<'}, {"&gt;", '>'}, {"&nbsp;", ' '}
            };
            std::string out;
            for (size_t i = 0; i < str.size(); ++i) {
                if (str[i] == '&') {
                    size_t semi = str.find(';', i);
                    if (semi != std::string::npos && semi - i <= 7) {
                        std::string entity = str.substr(i, semi - i + 1);
                        auto it = entities.find(entity);
                        if (it != entities.end()) {
                            out.push_back(it->second);
                            i = semi;
                            continue;
                        }
                    }
                }
                out.push_back(str[i]);
            }
            return {out.begin(), out.end()};
        }
    }
};

// --- ROT13 Transformer ---
class Rot13Transformer : public ITransformer {
public:
    [[nodiscard]] std::string getName() const override { return "ROT13"; }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        std::vector<uint8_t> out = input;
        for (auto& c : out) {
            if (c >= 'a' && c <= 'z') c = static_cast<uint8_t>('a' + (c - 'a' + 13) % 26);
            else if (c >= 'A' && c <= 'Z') c = static_cast<uint8_t>('A' + (c - 'A' + 13) % 26);
        }
        return out;
    }
};

// --- Line-Ending Transformer (CRLF <-> LF) ---
class LineEndingTransformer : public ITransformer {
private:
    bool toCrlf;

public:
    explicit LineEndingTransformer(bool crlf) : toCrlf(crlf) {}

    [[nodiscard]] std::string getName() const override {
        return toCrlf ? "Convert to CRLF" : "Convert to LF";
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        std::vector<uint8_t> out;
        out.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\r') {
                if (i + 1 < input.size() && input[i + 1] == '\n') {
                    if (toCrlf) {
                        out.push_back('\r');
                        out.push_back('\n');
                    } else {
                        out.push_back('\n');
                    }
                    i++;
                } else {
                    if (toCrlf) {
                        out.push_back('\r');
                        out.push_back('\n');
                    } else {
                        out.push_back('\n');
                    }
                }
            } else if (input[i] == '\n') {
                if (toCrlf) {
                    out.push_back('\r');
                    out.push_back('\n');
                } else {
                    out.push_back('\n');
                }
            } else {
                out.push_back(input[i]);
            }
        }
        return out;
    }
};

// ============================================================================
// 2. COMPOSITE PIPELINE & REGISTRY / REQUEST PARSER
// ============================================================================

class TransformerPipeline : public ITransformer {
private:
    std::vector<std::unique_ptr<ITransformer>> stages;

public:
    void addStage(std::unique_ptr<ITransformer> stage) {
        if (stage) stages.push_back(std::move(stage));
    }

    [[nodiscard]] bool empty() const { return stages.empty(); }

    [[nodiscard]] std::string getName() const override {
        std::string desc;
        for (size_t i = 0; i < stages.size(); ++i) {
            desc += stages[i]->getName();
            if (i + 1 < stages.size()) desc += " -> ";
        }
        return desc;
    }

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override {
        std::vector<uint8_t> current = input;
        for (const auto& stage : stages) {
            current = stage->transform(current);
        }
        return current;
    }
};

class RequestParser {
public:
    static std::optional<uint32_t> resolveCodePage(std::string_view name) {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        static const std::unordered_map<std::string, uint32_t> cpMap = {
            {"utf8", CP_UTF8}, {"utf-8", CP_UTF8}, {"u8", CP_UTF8},
            {"latin1", 28591}, {"iso-8859-1", 28591}, {"l1", 28591},
            {"windows-1252", 1252}, {"win1252", 1252}, {"cp1252", 1252}, {"ansi", 1252},
            {"ascii", 20127}, {"us-ascii", 20127}, {"us", 20127},
            {"dos", 437}, {"cp437", 437}, {"oem", 437}, {"ibmpc", 437},
            {"windows-1251", 1251}, {"cp1251", 1251}, {"cyrillic", 1251},
            {"windows-1250", 1250}, {"cp1250", 1250},
            {"shift-jis", 932}, {"sjis", 932}, {"cp932", 932}
        };

        auto it = cpMap.find(lower);
        if (it != cpMap.end()) return it->second;
        return std::nullopt;
    }

    static std::unique_ptr<ITransformer> createSurface(std::string_view name, bool encode) {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "b64" || lower == "base64" || lower == "64")
            return std::make_unique<Base64Transformer>(encode);
        if (lower == "hex" || lower == "base16" || lower == "16" || lower == "x")
            return std::make_unique<HexTransformer>(encode);
        if (lower == "url" || lower == "percent")
            return std::make_unique<UrlTransformer>(encode);
        if (lower == "html" || lower == "entity")
            return std::make_unique<HtmlEntityTransformer>(encode);
        if (lower == "rot13" || lower == "rot-13" || lower == "13")
            return std::make_unique<Rot13Transformer>();
        if (lower == "crlf" || lower == "dos" || lower == "win")
            return std::make_unique<LineEndingTransformer>(true);
        if (lower == "lf" || lower == "unix")
            return std::make_unique<LineEndingTransformer>(false);

        return nullptr;
    }

    static std::unique_ptr<TransformerPipeline> build(const std::string& requestStr) {
        auto pipeline = std::make_unique<TransformerPipeline>();

        // Surface shortcut syntax: e.g. "/rot13" or "/base64"
        if (requestStr.rfind("/", 0) == 0) {
            std::string surface = requestStr.substr(1);
            auto stage = createSurface(surface, true);
            if (!stage) return nullptr;
            pipeline->addStage(std::move(stage));
            return pipeline;
        }

        // Delimiter format: BEFORE..AFTER or ..AFTER
        size_t delimPos = requestStr.find("..");
        std::string before = (delimPos != std::string::npos) ? requestStr.substr(0, delimPos) : "";
        std::string after = (delimPos != std::string::npos) ? requestStr.substr(delimPos + 2) : requestStr;

        // Check if decoding a surface: e.g. "base64..utf8" or "base64.."
        if (!before.empty()) {
            if (auto decodeSurface = createSurface(before, false)) {
                pipeline->addStage(std::move(decodeSurface));
                before.clear();
            }
        }

        // Check if encoding a surface: e.g. "utf8..base64" or "..base64"
        if (!after.empty()) {
            if (auto encodeSurface = createSurface(after, true)) {
                // If 'before' has a charset, convert to UTF-8 first
                if (!before.empty()) {
                    auto fromCP = resolveCodePage(before);
                    if (fromCP.has_value() && *fromCP != CP_UTF8) {
                        pipeline->addStage(std::make_unique<WindowsCodePageTransformer>(*fromCP, CP_UTF8, before + "..UTF-8"));
                    }
                }
                pipeline->addStage(std::move(encodeSurface));
                return pipeline;
            }
        }

        // Standard Charset Conversion: BEFORE -> AFTER
        if (before.empty()) before = "latin1"; // GNU recode default source

        auto fromCP = resolveCodePage(before);
        auto toCP = resolveCodePage(after);

        if (fromCP.has_value() && toCP.has_value()) {
            pipeline->addStage(std::make_unique<WindowsCodePageTransformer>(*fromCP, *toCP, before + ".." + after));
            return pipeline;
        }

        // UTF-16 conversions
        std::string bLower = before, aLower = after;
        std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
        std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);

        if (bLower == "utf8" && (aLower == "utf16" || aLower == "utf-16le" || aLower == "u16")) {
            pipeline->addStage(std::make_unique<Utf16Transformer>(true));
            return pipeline;
        }
        if ((bLower == "utf16" || bLower == "utf-16le" || bLower == "u16") && aLower == "utf8") {
            pipeline->addStage(std::make_unique<Utf16Transformer>(false));
            return pipeline;
        }

        return nullptr;
    }
};

// ============================================================================
// 3. PIPING & STREAM HANDLER
// ============================================================================

class PipeStreamHandler {
public:
    static void configureBinaryMode() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    }

    static std::vector<uint8_t> readFromStdin() {
        configureBinaryMode();
        std::vector<uint8_t> buffer;
        char chunk[16384];
        while (std::cin.read(chunk, sizeof(chunk)) || std::cin.gcount() > 0) {
            buffer.insert(buffer.end(), chunk, chunk + std::cin.gcount());
        }
        return buffer;
    }

    static void writeToStdout(const std::vector<uint8_t>& data) {
        configureBinaryMode();
        std::cout.write(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout.flush();
    }
};

// ============================================================================
// 4. CONFIGURATION & HELP SYSTEM
// ============================================================================

struct RecodeOptions {
    bool verbose{false};
    bool force{false};
    bool listCharsets{false};
    std::string request;
    std::vector<std::string> fileList;

    static void showHelp() {
        std::cout << R"(recode(1)                  CrossShell for UNIX Reference Manual                 recode(1)

    NAME
        recode - convert files between character sets and surfaces

    SYNOPSIS
        recode [OPTIONS] REQUEST [FILE...]
        recode [OPTIONS] REQUEST < INPUT > OUTPUT

    DESCRIPTION
        recode converts character encodings and data surfaces between files or
        standard streams. It can transform between various code pages, UTF
        encodings, Base64, Hexadecimal, URL percent-encoding, HTML entities,
        and line endings.

    OPTIONS
        -l, --list
            List all supported character sets and surfaces.

        -v, --verbose
            Print each conversion step and transformation to standard error.

        -f, --force
            Force conversion even when characters cannot be represented exactly.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        recode ..base64 message.txt
            Base64 encode message.txt in place.

        recode base64.. < encoded.txt > decoded.bin
            Decode Base64 stream from standard input to standard output.

        recode l1..u8 document.txt
            Convert document.txt from Latin-1 (ISO-8859-1) to UTF-8.

        recode utf8..html < page.html > escaped.html
            Convert special characters in UTF-8 text to HTML entities.

        recode /rot13 < text.txt > cipher.txt
            Apply ROT13 cipher transformation to input text.

    CrossShell for UNIX                                                          recode(1)
)";
    }

    static void listSupported() {
        std::cout << "\nSupported Character Sets (Windows Native):\n";
        std::cout << "  - UTF-8 (u8, utf8, utf-8)\n";
        std::cout << "  - ISO-8859-1 (l1, latin1, iso-8859-1)\n";
        std::cout << "  - Windows-1252 (cp1252, win1252, ansi)\n";
        std::cout << "  - Windows-1251 (cp1251, cyrillic)\n";
        std::cout << "  - Windows-1250 (cp1250, Central European)\n";
        std::cout << "  - UTF-16LE (u16, utf16, utf-16le)\n";
        std::cout << "  - ASCII (ascii, us-ascii, us)\n";
        std::cout << "  - IBM-PC (cp437, dos, oem)\n";
        std::cout << "  - Shift-JIS (cp932, sjis, shift-jis)\n";
        std::cout << "\nSupported Surfaces:\n";
        std::cout << "  - Base64 (b64, base64)\n";
        std::cout << "  - Hexadecimal (hex, base16, 16)\n";
        std::cout << "  - URL Percent-Encoding (url, percent)\n";
        std::cout << "  - HTML Entities (html, entity)\n";
        std::cout << "  - ROT13 Substitution (/rot13, /13)\n";
        std::cout << "  - Line Endings (crlf, lf)\n\n";
    }

    static RecodeOptions parse(int argc, char* argv[]) {
        RecodeOptions opts;
        std::vector<std::string> args;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                showHelp();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opts.listCharsets = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
            } else {
                args.push_back(std::string(arg));
            }
        }

        if (opts.listCharsets) return opts;

        if (!args.empty()) {
            opts.request = args[0];
            for (size_t i = 1; i < args.size(); ++i) {
                opts.fileList.push_back(args[i]);
            }
        }

        return opts;
    }
};

// ============================================================================
// 5. APPLICATION ENGINE
// ============================================================================

class RecodeEngine {
private:
    RecodeOptions options;

public:
    explicit RecodeEngine(RecodeOptions opts) : options(std::move(opts)) {}

    int run() {
        if (options.listCharsets) {
            RecodeOptions::listSupported();
            return 0;
        }

        if (options.request.empty()) {
            std::cerr << "recode: missing conversion request.\n";
            std::cerr << "Try 'recode --help' for more information.\n";
            return 1;
        }

        auto pipeline = RequestParser::build(options.request);
        if (!pipeline || pipeline->empty()) {
            std::cerr << "recode: invalid request sequence '" << options.request << "'\n";
            return 1;
        }

        if (options.verbose) {
            std::cerr << "recode: active transformation pipeline [" << pipeline->getName() << "]\n";
        }

        // Pipe Mode: STDIN -> STDOUT
        if (options.fileList.empty() || (options.fileList.size() == 1 && options.fileList[0] == "-")) {
            auto inputData = PipeStreamHandler::readFromStdin();
            auto result = pipeline->transform(inputData);
            PipeStreamHandler::writeToStdout(result);
            return 0;
        }

        // In-Place File Mode
        int exitCode = 0;
        for (const auto& filePath : options.fileList) {
            if (!processFile(filePath, *pipeline)) {
                exitCode = 1;
            }
        }

        return exitCode;
    }

private:
    bool processFile(const fs::path& path, const TransformerPipeline& pipeline) {
        if (!fs::exists(path)) {
            std::cerr << "recode: " << path << ": No such file or directory\n";
            return false;
        }

        std::ifstream inFile(path, std::ios::binary);
        if (!inFile) {
            std::cerr << "recode: " << path << ": Permission denied\n";
            return false;
        }

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        auto output = pipeline.transform(buffer);

        // Write atomically using temporary file
        fs::path tempPath = path.string() + ".recode_tmp";
        std::ofstream outFile(tempPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "recode: failed to create temporary file for " << path << "\n";
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(output.data()), output.size());
        outFile.close();

        std::error_code ec;
        fs::rename(tempPath, path, ec);
        if (ec) {
            // Overwrite fallback if atomic swap is locked
            fs::copy_file(tempPath, path, fs::copy_options::overwrite_existing, ec);
            fs::remove(tempPath, ec);
        }

        if (options.verbose) {
            std::cerr << "recode: " << path << " successfully converted.\n";
        }

        return true;
    }
};

// ============================================================================
// 6. MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    try {
        RecodeOptions options = RecodeOptions::parse(argc, argv);
        RecodeEngine engine(options);
        return engine.run();
    } catch (const std::exception& ex) {
        std::cerr << "recode error: " << ex.what() << "\n";
        return 1;
    }
}