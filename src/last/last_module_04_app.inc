
static int last_main(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");

    int maxLines = -1;
    std::wstring evtxFile = L"";
    std::unordered_set<std::wstring> filterUsers;

    // CLI Arguments Parser
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == L"-n" && i + 1 < argc) {
            maxLines = _wtoi(argv[++i]);
        } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && iswdigit(arg[1])) {
            maxLines = _wtoi(arg.c_str() + 1);
        } else if (arg == L"-f" && i + 1 < argc) {
            evtxFile = argv[++i];
        } else if (arg[0] != L'-') {
            filterUsers.insert(argv[i]);
        }
    }

    if (evtxFile.empty() && !IsUserAdmin()) {
        std::cerr << "[!] WARNING: Administrator access required.\n"
                  << "    Please run in an Administrator terminal session.\n\n";
    }

    std::vector<EventRecord> events;

    if (!evtxFile.empty()) {
        LPCWSTR fileQuery = L"*[System[(EventID=4624 or EventID=4634 or EventID=4647 or EventID=6005 or EventID=6006 or EventID=1074)]]";
        QueryChannel(evtxFile.c_str(), fileQuery, true, events);
    } else {
        // Query live Security (Logons) and System (Reboots)
        LPCWSTR secQuery = L"*[System[(EventID=4624 or EventID=4634 or EventID=4647)]]";
        LPCWSTR sysQuery = L"*[System[(EventID=6005 or EventID=6006 or EventID=1074)]]";
        QueryChannel(L"Security", secQuery, false, events);
        QueryChannel(L"System", sysQuery, false, events);
    }

    if (events.empty()) {
        std::cout << "No matching login or system events found.\n";
        return 0;
    }

    // Sort chronologically ascending for session duration logic
    std::sort(events.begin(), events.end(), [](const EventRecord& a, const EventRecord& b) {
        return a.timestamp < b.timestamp;
    });

    ULONGLONG earliestTimestamp = events.front().timestamp;

    std::unordered_map<std::wstring, ULONGLONG> activeLogoffs;
    ULONGLONG lastShutdownTime = 0;
    std::vector<DisplayRecord> displayList;

    // Process from newest to oldest
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        const auto& rec = *it;

        if (rec.eventId == 6006 || rec.eventId == 1074) { // System Shutdown
            lastShutdownTime = rec.timestamp;
        } 
        else if (rec.eventId == 6005) { // System Boot
            DisplayRecord disp;
            disp.username = L"reboot";
            disp.tty = L"system boot";
            disp.host = L"";
            disp.loginTime = rec.timestamp;
            disp.logoutTime = lastShutdownTime;
            disp.stillLoggedIn = false;
            disp.stillRunning = (lastShutdownTime == 0);

            displayList.push_back(disp);
            lastShutdownTime = 0;
        } 
        else if (rec.eventId == 4634 || rec.eventId == 4647) { // Logoff
            if (!rec.logonId.empty()) {
                activeLogoffs[rec.logonId] = rec.timestamp;
            }
        } 
        else if (rec.eventId == 4624) { // Logon
            if (!filterUsers.empty()) {
                bool matched = false;
                for (const auto& u : filterUsers) {
                    if (_wcsicmp(rec.username.c_str(), u.c_str()) == 0) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) continue;
            } else if (IsSystemNoiseUser(rec.username)) {
                continue; // Skip system accounts
            }

            DisplayRecord disp;
            disp.username = rec.username;
            disp.tty = LogonTypeToTTY(rec.logonType);
            disp.host = rec.ipOrHost;
            disp.loginTime = rec.timestamp;
            disp.stillRunning = false;
            disp.wasShutdownTerminated = false;

            auto lIt = activeLogoffs.find(rec.logonId);
            if (lIt != activeLogoffs.end()) {
                disp.logoutTime = lIt->second;
                disp.stillLoggedIn = false;
                activeLogoffs.erase(lIt);
            } else {
                disp.logoutTime = 0;
                disp.stillLoggedIn = true;
            }

            // Terminate session with shutdown time if it crossed a reboot/shutdown
            if (disp.stillLoggedIn && lastShutdownTime != 0 && disp.loginTime < lastShutdownTime) {
                disp.stillLoggedIn = false;
                disp.logoutTime = lastShutdownTime;
                disp.wasShutdownTerminated = true;
            }

            displayList.push_back(disp);
        }
    }

    if (maxLines > 0 && (size_t)maxLines < displayList.size()) {
        displayList.resize(maxLines);
    }

    for (const auto& disp : displayList) {
        std::string uStr = WStrToStr(disp.username);
        std::string tStr = WStrToStr(disp.tty);
        std::string hStr = WStrToStr(disp.host);
        std::string loginStr = FormatTime(disp.loginTime);

        std::cout << std::left 
                  << std::setw(16) << uStr 
                  << std::setw(12) << tStr 
                  << std::setw(16) << hStr 
                  << loginStr << " - ";

        if (disp.stillLoggedIn) {
            std::cout << "still logged in\n";
        } else if (disp.stillRunning) {
            std::cout << "still running\n";
        } else {
            FILETIME ftEnd;
            ftEnd.dwLowDateTime = (DWORD)(disp.logoutTime & 0xFFFFFFFF);
            ftEnd.dwHighDateTime = (DWORD)(disp.logoutTime >> 32);

            SYSTEMTIME stUTC, stLocal;
            FileTimeToSystemTime(&ftEnd, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", stLocal.wHour, stLocal.wMinute);

            std::string durStr = FormatDuration(disp.loginTime, disp.logoutTime);
            if (disp.wasShutdownTerminated) {
                std::cout << "down" << "  " << durStr << "\n";
            } else {
                std::cout << timeBuf << "  " << durStr << "\n";
            }
        }
    }

    std::cout << "\nwtmp begins " << FormatTime(earliestTimestamp) << "\n";
    return 0;
}

class LastApplication { public: int run(int argc, wchar_t* argv[]) const { return last_main(argc, argv); } };
