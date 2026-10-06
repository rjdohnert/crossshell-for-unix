#include "engine.hpp"
#include "terminal.hpp"

static void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE && hErr != NULL) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

std::string TcshEngine::formatPrompt(const std::string& pattern) const {
    std::string res = pattern;

    wchar_t currentDirW[MAX_PATH] = { 0 };
    GetCurrentDirectoryW(MAX_PATH, currentDirW);
    std::string cwd = wstring_to_string(currentDirW);

    std::string cwdTilde = cwd;
    if (cwd.find(homeDir) == 0) {
        cwdTilde = "~" + cwd.substr(homeDir.length());
    }

    wchar_t userW[256] = { 0 };
    DWORD userLen = 256;
    GetUserNameW(userW, &userLen);
    std::string username = wstring_to_string(userW);

    wchar_t userDomainW[256] = { 0 };
    std::string userDomain;
    DWORD domainLen = GetEnvironmentVariableW(L"USERDNSDOMAIN", userDomainW, ARRAYSIZE(userDomainW));
    if (domainLen > 0 && domainLen < ARRAYSIZE(userDomainW)) {
        userDomain = wstring_to_string(userDomainW);
    } else {
        domainLen = GetEnvironmentVariableW(L"USERDOMAIN", userDomainW, ARRAYSIZE(userDomainW));
        if (domainLen > 0 && domainLen < ARRAYSIZE(userDomainW)) {
            userDomain = wstring_to_string(userDomainW);
        }
    }

    wchar_t hostW[256] = { 0 };
    DWORD hostLen = 256;
    GetComputerNameW(hostW, &hostLen);
    std::string fullHostname = wstring_to_string(hostW);
    std::string shortHostname = fullHostname;
    size_t dotPos = shortHostname.find('.');
    if (dotPos != std::string::npos) {
        shortHostname = shortHostname.substr(0, dotPos);
    }

    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    std::string promptChar = isAdmin ? "#" : "%";

    if (res == "> " || res == "# " || res == "% ") {
        return promptChar + " ";
    }

    std::string statusStr = "0";
    auto itStatus = variables.find("status");
    if (itStatus != variables.end() && !itStatus->second.empty()) {
        statusStr = itStatus->second[0];
    }

    size_t pos;
    while ((pos = res.find("%~")) != std::string::npos) res.replace(pos, 2, cwdTilde);
    while ((pos = res.find("%/")) != std::string::npos) res.replace(pos, 2, cwd);
    while ((pos = res.find("%c")) != std::string::npos) {
        size_t p = cwd.find_last_of("/\\");
        std::string tail = (p != std::string::npos) ? cwd.substr(p + 1) : cwd;
        res.replace(pos, 2, tail);
    }
    while ((pos = res.find("%n")) != std::string::npos) res.replace(pos, 2, username);
    while ((pos = res.find("%d")) != std::string::npos) res.replace(pos, 2, userDomain);
    while ((pos = res.find("%m")) != std::string::npos) res.replace(pos, 2, shortHostname);
    while ((pos = res.find("%M")) != std::string::npos) res.replace(pos, 2, fullHostname);
    while ((pos = res.find("%#")) != std::string::npos) res.replace(pos, 2, promptChar);
    while ((pos = res.find("%?")) != std::string::npos) res.replace(pos, 2, statusStr);
    while ((pos = res.find("%%")) != std::string::npos) res.replace(pos, 2, "%");

    return res;
}


void TcshEngine::handleTabCompletion(std::string& currentBuffer, size_t& cursorIndex) {
    // Find the token being completed.
    size_t stem_start = cursorIndex;
    while (stem_start > 0 && !isspace((unsigned char)currentBuffer[stem_start-1])) --stem_start;
    std::string stem = currentBuffer.substr(stem_start, cursorIndex - stem_start);

    auto str_low = [](std::string s) -> std::string {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
        return s;
    };

    auto common_pfx = [](const std::vector<std::string>& v) -> std::string {
        if (v.empty()) return "";
        std::string p = v[0];
        for (size_t i = 1; i < v.size(); ++i) {
            size_t j = 0;
            while (j < p.size() && j < v[i].size() && tolower((unsigned char)p[j]) == tolower((unsigned char)v[i][j])) ++j;
            p = p.substr(0, j);
        }
        return p;
    };

    if (!compActive) {
        // Determine context: first token = command, otherwise = path.
        bool is_cmd = (stem_start == 0) ||
                      currentBuffer.substr(0, stem_start).find_first_not_of(" \t") == std::string::npos;
        bool has_sep = stem.find_first_of("/\\") != std::string::npos;

        std::vector<std::string> cands;
        std::set<std::string> seen;

        if (is_cmd && !has_sep) {
            // Command completion: builtins + aliases + PATH executables.
            std::string p_low = str_low(stem);
            for (const auto& b : builtins) {
                if (str_low(b).substr(0, p_low.size()) == p_low && !seen.count(b))
                    { cands.push_back(b); seen.insert(b); }
            }
            for (const auto& [k,v] : aliases) {
                if (str_low(k).substr(0, p_low.size()) == p_low && !seen.count(k))
                    { cands.push_back(k); seen.insert(k); }
            }
            char* pe = getenv("PATH");
            if (pe) {
                std::istringstream ss(pe); std::string dir;
                while (std::getline(ss, dir, ';')) {
                    WIN32_FIND_DATAW fd;
                    std::wstring pat = string_to_wstring(dir + "\\" + stem + "*");
                    HANDLE hf = FindFirstFileW(pat.c_str(), &fd);
                    if (hf == INVALID_HANDLE_VALUE) continue;
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        std::string name = wstring_to_string(fd.cFileName);
                        std::string base = name;
                        if (base.size() > 4) {
                            std::string ext = str_low(base.substr(base.size()-4));
                            if (ext==".exe"||ext==".com"||ext==".bat"||ext==".cmd")
                                base = base.substr(0, base.size()-4);
                        }
                        if (!seen.count(base)) { cands.push_back(base); seen.insert(base); }
                    } while (FindNextFileW(hf, &fd));
                    FindClose(hf);
                }
            }
            if (cands.empty()) is_cmd = false; // fall through to path
        }

        if (!is_cmd || has_sep) {
            // Path completion.
            std::string dir_part, file_part;
            size_t sep = stem.find_last_of("/\\");
            if (sep == std::string::npos) { dir_part = ".\\*"; file_part = stem; }
            else { dir_part = stem.substr(0, sep+1) + "*"; file_part = stem.substr(sep+1); }
            std::string search = (sep == std::string::npos) ? stem + "*" : stem.substr(0, sep+1) + file_part + "*";
            WIN32_FIND_DATAW fd;
            HANDLE hf = FindFirstFileW(string_to_wstring(search).c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring wn(fd.cFileName);
                    if (wn == L"." || wn == L"..") continue;
                    std::string match = wstring_to_string(fd.cFileName);
                    std::string full = (sep == std::string::npos) ? match : stem.substr(0, sep+1) + match;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) full += "/";
                    if (!seen.count(full)) { cands.push_back(full); seen.insert(full); }
                } while (FindNextFileW(hf, &fd));
                FindClose(hf);
            }
        }

        std::sort(cands.begin(), cands.end());
        compCandidates = cands;
        compStem     = stem;
        compStemPos  = stem_start;
        compIdx      = 0;

        if (cands.empty()) { putchar('\a'); fflush(stdout); return; }

        std::string cp = common_pfx(cands);
        currentBuffer.replace(stem_start, cursorIndex - stem_start, cp);
        cursorIndex = stem_start + cp.size();

        if (cands.size() == 1) {
            if (!currentBuffer.empty() && currentBuffer.back() != '/' && currentBuffer.back() != '\\')
                { currentBuffer.insert(cursorIndex, 1, ' '); cursorIndex++; }
            compActive = false;
        } else {
            compActive = true;
            compStem   = cp;
            // Print candidates below.
            std::string list = "\n";
            for (size_t i = 0; i < cands.size(); ++i)
                list += cands[i] + (i+1 < cands.size() ? "  " : "\n");
            fwrite(list.data(), 1, list.size(), stdout);
            fflush(stdout);
        }
    } else {
        // Cycle through candidates.
        const std::string& cand = compCandidates[compIdx];
        currentBuffer.replace(compStemPos, cursorIndex - compStemPos, cand);
        cursorIndex = compStemPos + cand.size();
        compIdx = (compIdx + 1) % compCandidates.size();
    }
}

void TcshEngine::redrawLine(HANDLE hConsole, COORD& startPos, const std::string& prompt, const std::string& buffer, size_t cursorIndex) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    SHORT bufferWidth = (csbi.dwSize.X > 0) ? csbi.dwSize.X : 80;

    std::string formattedPrompt = formatPrompt(prompt);
    std::string fullLine = formattedPrompt + buffer;
    std::wstring wFullLine = string_to_wstring(fullLine);
    DWORD written;

    SetConsoleCursorPosition(hConsole, startPos);

    size_t clearLength = (lastRedrawLineLength > fullLine.length()) ? lastRedrawLineLength : fullLine.length();
    DWORD totalClearLength = static_cast<DWORD>(clearLength + static_cast<size_t>(bufferWidth));
    FillConsoleOutputCharacterW(hConsole, L' ', totalClearLength, startPos, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, totalClearLength, startPos, &written);

    SetConsoleCursorPosition(hConsole, startPos);
    WriteConsoleW(hConsole, wFullLine.c_str(), static_cast<DWORD>(wFullLine.length()), &written, NULL);
    lastRedrawLineLength = fullLine.length();

    SHORT totalOffset = static_cast<SHORT>(formattedPrompt.length() + cursorIndex);
    COORD targetPos;
    targetPos.Y = startPos.Y + (startPos.X + totalOffset) / bufferWidth;
    targetPos.X = (startPos.X + totalOffset) % bufferWidth;

    if (targetPos.Y >= csbi.dwSize.Y) targetPos.Y = csbi.dwSize.Y - 1;

    SetConsoleCursorPosition(hConsole, targetPos);
}

std::string TcshEngine::readLineWithEditing() {
    updateJobs();
    std::cout.flush();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD startPos = csbi.dwCursorPosition;

    std::string buffer = "";
    size_t cursorIndex = 0;
    historyIndex = history.size();
    lastRedrawLineLength = 0;

    redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);

    while (true) {
        if (g_interrupted.load(std::memory_order_relaxed)) {
            g_interrupted.store(false, std::memory_order_relaxed);
            std::string trapCommand;
            auto trapIt = trapHandlers.find("INT");
            if (trapIt == trapHandlers.end()) trapIt = trapHandlers.find("BREAK");
            if (trapIt != trapHandlers.end()) trapCommand = trapIt->second;

            buffer = "";
            cursorIndex = 0;
            std::cout << "\n";
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            startPos = csbi.dwCursorPosition;
            if (!trapCommand.empty()) {
                executeCommandLine(trapCommand);
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        }

        int ch = _getch();
        if (ch == 13) { std::cout << "\n"; compActive = false; compCandidates.clear(); break; }
        else if (ch == 8 && cursorIndex > 0) {
            compActive = false; compCandidates.clear();
            buffer.erase(cursorIndex - 1, 1);
            cursorIndex--;
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch == 9) {
            bool wasActive = compActive;
            handleTabCompletion(buffer, cursorIndex);
            if (!wasActive && compActive) {
                // Candidate list was just printed; re-anchor so redrawLine targets the new row.
                GetConsoleScreenBufferInfo(hConsole, &csbi);
                startPos = csbi.dwCursorPosition;
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch == 0 || ch == 224) {
            compActive = false; compCandidates.clear();
            int code = _getch();
            if (code == 75 && cursorIndex > 0) cursorIndex--;
            else if (code == 77 && cursorIndex < buffer.length()) cursorIndex++;
            else if (code == 71) cursorIndex = 0;
            else if (code == 79) cursorIndex = buffer.length();
            else if (code == 72 && !history.empty() && historyIndex > 0) {
                buffer = history[--historyIndex];
                cursorIndex = buffer.length();
            } else if (code == 80) {
                if (historyIndex + 1 < history.size()) buffer = history[++historyIndex];
                else { historyIndex = history.size(); buffer = ""; }
                cursorIndex = buffer.length();
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch >= 32 && ch <= 126) {
            compActive = false; compCandidates.clear();
            buffer.insert(cursorIndex, 1, static_cast<char>(ch));
            cursorIndex++;
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        }
    }

    return buffer;
}

