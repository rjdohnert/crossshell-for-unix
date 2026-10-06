#include "engine.hpp"

std::wstring SystemNodeManager::QueryName(NameQueryMode mode) {
    COMPUTER_NAME_FORMAT format = ComputerNameDnsFullyQualified;
    switch (mode) {
        case NameQueryMode::ShortName:
            format = ComputerNameDnsHostname;
            break;
        case NameQueryMode::DomainOnly:
            format = ComputerNameDnsDomain;
            break;
        case NameQueryMode::FullyQualified:
        default:
            format = ComputerNameDnsFullyQualified;
            break;
    }

    DWORD size = 0;
    GetComputerNameExW(format, nullptr, &size);
    if (size == 0) {
        return L"";
    }

    std::wstring buffer(size, L'\0');
    if (GetComputerNameExW(format, &buffer[0], &size)) {
        buffer.resize(size);
        return buffer;
    }
    return L"";
}

bool SystemNodeManager::SetNodeName(const std::wstring& newHostname, DWORD& outError) {
    outError = 0;
    if (SetComputerNameExW(ComputerNamePhysicalDnsHostname, newHostname.c_str())) {
        return true;
    }
    outError = GetLastError();
    return false;
}
