#include "windows_code_page_transformer.hpp"

WindowsCodePageTransformer::WindowsCodePageTransformer(uint32_t fromCP, uint32_t toCP, std::string transformerName)
        : fromCodePage(fromCP), toCodePage(toCP), name(std::move(transformerName)) {}

[[nodiscard]] std::string WindowsCodePageTransformer::getName() const  { return name; }

[[nodiscard]] std::vector<uint8_t> WindowsCodePageTransformer::transform(const std::vector<uint8_t>& input) const  {
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
