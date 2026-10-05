std::string WStringToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        &out[0],
        required,
        nullptr,
        nullptr);

    if (written <= 0) {
        return {};
    }

    return out;
}

bool IsRealNetworkInterface(ULONG ifType, const std::string& friendlyName) {
    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }

    switch (ifType) {
        case IF_TYPE_ETHERNET_CSMACD:
        case IF_TYPE_IEEE80211:
            break;
        default:
            return false;
    }

    std::string text = friendlyName;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const bool synthetic =
        text.find("wfp") != std::string::npos ||
        text.find("qos packet scheduler") != std::string::npos ||
        text.find("teredo") != std::string::npos ||
        text.find("6to4") != std::string::npos ||
        text.find("ip-https") != std::string::npos ||
        text.find("isatap") != std::string::npos ||
        text.find("pseudo-interface") != std::string::npos ||
        text.find("virtual wifi filter driver") != std::string::npos ||
        text.find("native mac layer lightweight filter") != std::string::npos ||
        text.find("802.3 mac layer lightweight filter") != std::string::npos ||
        text.find("kernel debugger") != std::string::npos ||
        text.find("vpn") != std::string::npos ||
        text.find("ras") != std::string::npos ||
        text.find("microsoft") != std::string::npos;

    return !synthetic;
}

