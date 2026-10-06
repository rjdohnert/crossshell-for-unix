#include "duration_parser.hpp"

std::string DurationParser::wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

bool DurationParser::parseSleepArgument(const std::wstring& arg, double& totalMs) {
    if (arg.empty()) return false;

    wchar_t* end_ptr = nullptr;
    double value = std::wcstod(arg.c_str(), &end_ptr);

    if (value < 0) {
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    if (end_ptr == arg.c_str()) {
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    double multiplier = 1000.0; // Default seconds -> ms
    if (*end_ptr != L'\0') {
        std::wstring suffix(end_ptr);
        for (auto& c : suffix) c = static_cast<wchar_t>(std::towlower(c));

        if (suffix == L"s" || suffix == L"sec" || suffix == L"secs") {
            multiplier = 1000.0;
        } else if (suffix == L"m" || suffix == L"min" || suffix == L"mins") {
            multiplier = 60.0 * 1000.0;
        } else if (suffix == L"h" || suffix == L"hr" || suffix == L"hrs" || suffix == L"hour" || suffix == L"hours") {
            multiplier = 3600.0 * 1000.0;
        } else if (suffix == L"d" || suffix == L"day" || suffix == L"days") {
            multiplier = 86400.0 * 1000.0;
        } else if (suffix == L"ms" || suffix == L"msec" || suffix == L"millis") {
            multiplier = 1.0;
        } else if (suffix == L"us" || suffix == L"usec" || suffix == L"micros") {
            multiplier = 0.001;
        } else {
            std::wcerr << L"sleep: unknown time unit -- '" << suffix << L"'\n";
            return false;
        }
    }

    totalMs += (value * multiplier);
    return true;
}
