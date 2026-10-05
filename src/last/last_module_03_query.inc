
void QueryChannel(LPCWSTR channelOrFile, LPCWSTR xpathQuery, bool isFile, std::vector<EventRecord>& records) {
    DWORD flags = isFile ? EvtQueryFilePath : EvtQueryChannelPath;
    flags |= EvtQueryReverseDirection;

    ScopedEvtHandle hQuery(EvtQuery(NULL, channelOrFile, xpathQuery, flags));
    if (!hQuery.get()) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            std::cerr << "[ERROR] Access Denied opening log: " << WStrToStr(channelOrFile)
                      << "\nPlease run this terminal session as Administrator.\n";
        }
        return;
    }

    EVT_HANDLE hEvents[100];
    DWORD dwReturned = 0;
    DWORD dwBufferSize = 16384;
    std::vector<wchar_t> buffer(dwBufferSize);

    while (EvtNext(hQuery, 100, hEvents, INFINITE, 0, &dwReturned)) {
        for (DWORD i = 0; i < dwReturned; i++) {
            ScopedEvtHandle hEvent(hEvents[i]);
            DWORD dwBufferUsed = 0;
            DWORD dwPropertyCount = 0;

            // Render Event XML cleanly (never fails on missing optional properties)
            if (!EvtRender(NULL, hEvent, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    buffer.resize(dwBufferUsed / sizeof(wchar_t) + 1);
                    if (!EvtRender(NULL, hEvent, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                        continue;
                    }
                } else {
                    continue;
                }
            }

            std::wstring xml(buffer.data());

            EventRecord rec = {};
            std::wstring idStr = ExtractXmlTag(xml, L"EventID");
            rec.eventId = idStr.empty() ? 0 : _wtoi(idStr.c_str());

            std::wstring timeStr = ExtractXmlAttr(xml, L"TimeCreated", L"SystemTime");
            rec.timestamp = ISO8601ToTimestamp(timeStr);

            rec.username = ExtractXmlData(xml, L"TargetUserName");
            rec.logonId = ExtractXmlData(xml, L"TargetLogonId");
            std::transform(rec.logonId.begin(), rec.logonId.end(), rec.logonId.begin(), ::towlower);

            std::wstring lTypeStr = ExtractXmlData(xml, L"LogonType");
            rec.logonType = lTypeStr.empty() ? 0 : _wtoi(lTypeStr.c_str());

            std::wstring ipStr = ExtractXmlData(xml, L"IpAddress");
            std::wstring wsStr = ExtractXmlData(xml, L"WorkstationName");

            if (!ipStr.empty() && ipStr != L"-" && ipStr != L"::1") {
                rec.ipOrHost = ipStr;
            } else if (!wsStr.empty() && wsStr != L"-") {
                rec.ipOrHost = wsStr;
            } else {
                rec.ipOrHost = L"127.0.0.1";
            }

            records.push_back(rec);
        }
    }
}

void PrintUsage(const wchar_t* progName) {
    std::wcout << L"Usage: " << progName << L" [-n number] [-f file] [user ...]\n"
               << L"Options:\n"
               << L"  -n <number>   Limit output to the specified number of lines.\n"
               << L"  -f <file>     Read from an exported .evtx log file instead of system event log.\n"
               << L"  -h, --help    Show this help message.\n";
}
