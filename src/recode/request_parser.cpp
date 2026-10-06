#include "base64_transformer.hpp"
#include "hex_transformer.hpp"
#include "html_entity_transformer.hpp"
#include "line_ending_transformer.hpp"
#include "request_parser.hpp"
#include "rot13_transformer.hpp"
#include "transformer_pipeline.hpp"
#include "transformer.hpp"
#include "url_transformer.hpp"
#include "utf16_transformer.hpp"
#include "windows_code_page_transformer.hpp"

std::optional<uint32_t> RequestParser::resolveCodePage(std::string_view name) {
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

std::unique_ptr<ITransformer> RequestParser::createSurface(std::string_view name, bool encode) {
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

std::unique_ptr<TransformerPipeline> RequestParser::build(const std::string& requestStr) {
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
