#include "utf16_transformer.hpp"

Utf16Transformer::Utf16Transformer(bool to16) : toUtf16(to16) {}

[[nodiscard]] std::string Utf16Transformer::getName() const  {
        return toUtf16 ? "UTF-8..UTF-16LE" : "UTF-16LE..UTF-8";
    }

[[nodiscard]] std::vector<uint8_t> Utf16Transformer::transform(const std::vector<uint8_t>& input) const  {
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
