/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// ============================================================================
// 1. DATA MODEL: ProcessRecord
// ============================================================================
class ProcessRecord {
public:
    DWORD pid = 0;
    DWORD ppid = 0;
    DWORD threads = 0;
    int priority = 0;
    std::wstring name;
    std::wstring user = L"SYSTEM";
    std::wstring status = L"Running";
    size_t workingSetKB = 0;      // Physical memory / RSS
    size_t virtualSizeKB = 0;     // VSZ / Pagefile usage
    std::wstring startTime = L"-";
    std::wstring cpuTime = L"00:00:00";

    // Dynamic field resolver for custom column selections (-o / --format)
    std::wstring getField(const std::wstring& key) const {
        std::wstring k = key;
        std::transform(k.begin(), k.end(), k.begin(), ::towlower);

        if (k == L"user" || k == L"owner")      return user;
        if (k == L"pid")                        return std::to_wstring(pid);
        if (k == L"ppid")                       return std::to_wstring(ppid);
        if (k == L"mem" || k == L"rss" || k == L"pmem") {
            if (workingSetKB >= 1024) {
                wchar_t buf[32];
                swprintf_s(buf, L"%.1f MB", workingSetKB / 1024.0);
                return buf;
            }
            return std::to_wstring(workingSetKB) + L" KB";
        }
        if (k == L"vsz" || k == L"vsize")       return std::to_wstring(virtualSizeKB) + L" KB";
        if (k == L"cpu" || k == L"time" || k == L"cputime") return cpuTime;
        if (k == L"threads" || k == L"thcnt" || k == L"nlwp") return std::to_wstring(threads);
        if (k == L"pri" || k == L"priority")    return std::to_wstring(priority);
        if (k == L"stat" || k == L"state")      return status;
        if (k == L"stime" || k == L"start")     return startTime;
        if (k == L"comm" || k == L"command" || k == L"cmd" || k == L"name") return name;

        return L"-";
    }
};

// ============================================================================
// 2. PROCESS COLLECTOR SERVICE (Win32 API Bridge)
// ============================================================================
class ProcessCollector {
public:
    static std::vector<ProcessRecord> collect() {
        std::vector<ProcessRecord> records;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return records;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &pe)) {
            do {
                ProcessRecord rec;
                rec.pid = pe.th32ProcessID;
                rec.ppid = pe.th32ParentProcessID;
                rec.threads = pe.cntThreads;
                rec.priority = pe.pcPriClassBase;
                rec.name = pe.szExeFile;

                enrichProcessData(rec);
                records.push_back(rec);
            } while (Process32NextW(snapshot, &pe));
        }

        CloseHandle(snapshot);
        return records;
    }

private:
    static void enrichProcessData(ProcessRecord& rec) {
        if (rec.pid == 0) {
            rec.name = L"[System Idle Process]";
            rec.user = L"NT AUTHORITY\\SYSTEM";
            return;
        }

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, rec.pid);
        if (!hProc) {
            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rec.pid);
        }

        if (hProc) {
            // Memory Info
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                rec.workingSetKB = pmc.WorkingSetSize / 1024;
                rec.virtualSizeKB = pmc.PagefileUsage / 1024;
            }

            // CPU & Start Times
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                rec.startTime = formatFileTime(ftCreate);
                rec.cpuTime = formatDuration(ftKernel, ftUser);
            }

            // Token Owner / Username Resolution
            HANDLE hToken = nullptr;
            if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
                DWORD len = 0;
                GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
                if (len > 0) {
                    std::vector<BYTE> buf(len);
                    if (GetTokenInformation(hToken, TokenUser, buf.data(), len, &len)) {
                        auto pUser = reinterpret_cast<TOKEN_USER*>(buf.data());
                        WCHAR name[256], domain[256];
                        DWORD nLen = 256, dLen = 256;
                        SID_NAME_USE use;
                        if (LookupAccountSidW(nullptr, pUser->User.Sid, name, &nLen, domain, &dLen, &use)) {
                            rec.user = std::wstring(domain) + L"\\" + name;
                        }
                    }
                }
                CloseHandle(hToken);
            }
            CloseHandle(hProc);
        }
    }

    static std::wstring formatFileTime(const FILETIME& ft) {
        FILETIME localFt;
        FileTimeToLocalFileTime(&ft, &localFt);
        SYSTEMTIME st;
        FileTimeToSystemTime(&localFt, &st);
        wchar_t buf[64];
        swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
        return buf;
    }

    static std::wstring formatDuration(const FILETIME& kernel, const FILETIME& user) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime;   k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;       u.HighPart = user.dwHighDateTime;
        unsigned long long total100ns = k.QuadPart + u.QuadPart;
        unsigned long long totalSec = total100ns / 10000000ULL;

        unsigned long long hrs = totalSec / 3600;
        unsigned long long mins = (totalSec % 3600) / 60;
        unsigned long long secs = totalSec % 60;

        wchar_t buf[64];
        swprintf_s(buf, L"%02llu:%02llu:%02llu", hrs, mins, secs);
        return buf;
    }
};

// ============================================================================
// 3. FORMATTER STRATEGIES (Table, CSV, JSON, XML)
// ============================================================================
class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) = 0;
};

class TableFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override {
        if (columns.empty()) return;

        std::map<std::wstring, size_t> widths;
        for (const auto& col : columns) {
            widths[col] = col.length();
        }

        for (const auto& r : records) {
            for (const auto& col : columns) {
                widths[col] = (std::max)(widths[col], r.getField(col).length());
            }
        }

        // Header
        for (size_t i = 0; i < columns.size(); ++i) {
            std::wstring colUpper = columns[i];
            std::transform(colUpper.begin(), colUpper.end(), colUpper.begin(), ::towupper);
            std::wcout << std::left << std::setw(widths[columns[i]] + 2) << colUpper;
        }
        std::wcout << L"\n";

        // Underline Separator
        for (size_t i = 0; i < columns.size(); ++i) {
            std::wcout << std::wstring(widths[columns[i]], L'-') << L"  ";
        }
        std::wcout << L"\n";

        // Rows
        for (const auto& r : records) {
            for (size_t i = 0; i < columns.size(); ++i) {
                std::wcout << std::left << std::setw(widths[columns[i]] + 2) << r.getField(columns[i]);
            }
            std::wcout << L"\n";
        }
    }
};

class CsvFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override {
        for (size_t i = 0; i < columns.size(); ++i) {
            std::wcout << L"\"" << columns[i] << L"\"" << (i + 1 < columns.size() ? L"," : L"\n");
        }
        for (const auto& r : records) {
            for (size_t i = 0; i < columns.size(); ++i) {
                std::wstring val = r.getField(columns[i]);
                size_t pos = 0;
                while ((pos = val.find(L"\"", pos)) != std::wstring::npos) {
                    val.replace(pos, 1, L"\"\"");
                    pos += 2;
                }
                std::wcout << L"\"" << val << L"\"" << (i + 1 < columns.size() ? L"," : L"\n");
            }
        }
    }
};

class JsonFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override {
        std::wcout << L"[\n";
        for (size_t i = 0; i < records.size(); ++i) {
            std::wcout << L"  {\n";
            for (size_t j = 0; j < columns.size(); ++j) {
                std::wstring val = records[i].getField(columns[j]);
                std::wcout << L"    \"" << columns[j] << L"\": \"" << escapeJson(val) << L"\""
                           << (j + 1 < columns.size() ? L",\n" : L"\n");
            }
            std::wcout << L"  }" << (i + 1 < records.size() ? L",\n" : L"\n");
        }
        std::wcout << L"]\n";
    }

private:
    static std::wstring escapeJson(const std::wstring& s) {
        std::wstring out;
        for (wchar_t c : s) {
            if (c == L'\"') out += L"\\\"";
            else if (c == L'\\') out += L"\\\\";
            else if (c == L'\b') out += L"\\b";
            else if (c == L'\f') out += L"\\f";
            else if (c == L'\n') out += L"\\n";
            else if (c == L'\r') out += L"\\r";
            else if (c == L'\t') out += L"\\t";
            else out += c;
        }
        return out;
    }
};

class XmlFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override {
        std::wcout << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        std::wcout << L"<processes count=\"" << records.size() << L"\">\n";
        for (const auto& r : records) {
            std::wcout << L"  <process>\n";
            for (const auto& col : columns) {
                std::wcout << L"    <" << col << L">"
                           << escapeXml(r.getField(col))
                           << L"</" << col << L">\n";
            }
            std::wcout << L"  </process>\n";
        }
        std::wcout << L"</processes>\n";
    }

private:
    static std::wstring escapeXml(const std::wstring& s) {
        std::wstring out;
        for (wchar_t c : s) {
            if (c == L'&') out += L"&amp;";
            else if (c == L'<') out += L"&lt;";
            else if (c == L'>') out += L"&gt;";
            else if (c == L'\"') out += L"&quot;";
            else if (c == L'\'') out += L"&apos;";
            else out += c;
        }
        return out;
    }
};

// ============================================================================
// 4. CLI CONFIG & PARSER
// ============================================================================
enum class OutputMode { TABLE, CSV, JSON, XML };

struct PsConfig {
    bool showAll = false;
    bool fullFormat = false;
    bool longFormat = false;
    bool showHelp = false;
    bool parseError = false;
    OutputMode mode = OutputMode::TABLE;
    std::wstring userFilter;
    std::set<DWORD> pidFilter;
    std::vector<std::wstring> customColumns;
};

class CommandLineParser {
public:
    static PsConfig parse(int argc, wchar_t* argv[]) {
        PsConfig cfg;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                cfg.showHelp = true;
                return cfg;
            } else if (arg == L"-e" || arg == L"-A" || arg == L"-a") {
                cfg.showAll = true;
            } else if (arg == L"-f") {
                cfg.fullFormat = true;
            } else if (arg == L"-l") {
                cfg.longFormat = true;
            } else if (arg == L"--json") {
                cfg.mode = OutputMode::JSON;
            } else if (arg == L"--csv") {
                cfg.mode = OutputMode::CSV;
            } else if (arg == L"--xml") {
                cfg.mode = OutputMode::XML;
            } else if (arg == L"--table") {
                cfg.mode = OutputMode::TABLE;
            } else if (arg == L"--output" && i + 1 < argc) {
                std::wstring val = argv[++i];
                if (val == L"json") cfg.mode = OutputMode::JSON;
                else if (val == L"csv") cfg.mode = OutputMode::CSV;
                else if (val == L"xml") cfg.mode = OutputMode::XML;
                else if (val == L"table") cfg.mode = OutputMode::TABLE;
                else cfg.parseError = true;
            } else if (arg.rfind(L"--output=", 0) == 0) {
                std::wstring val = arg.substr(9);
                if (val == L"json") cfg.mode = OutputMode::JSON;
                else if (val == L"csv") cfg.mode = OutputMode::CSV;
                else if (val == L"xml") cfg.mode = OutputMode::XML;
                else if (val == L"table") cfg.mode = OutputMode::TABLE;
                else cfg.parseError = true;
            } else if ((arg == L"-u" || arg == L"--user") && i + 1 < argc) {
                cfg.userFilter = argv[++i];
            } else if ((arg == L"-p" || arg == L"-q" || arg == L"--pid") && i + 1 < argc) {
                std::wstringstream ss(argv[++i]);
                std::wstring token;
                while (std::getline(ss, token, L',')) {
                    try { cfg.pidFilter.insert(std::stoul(token)); } catch (...) {}
                }
            } else if ((arg == L"-o" || arg == L"--format") && i + 1 < argc) {
                std::wstringstream ss(argv[++i]);
                std::wstring token;
                while (std::getline(ss, token, L',')) {
                    token.erase(0, token.find_first_not_of(L" \t"));
                    token.erase(token.find_last_not_of(L" \t") + 1);
                    if (!token.empty()) cfg.customColumns.push_back(token);
                }
            }
        }
        return cfg;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================
class PsApplication {
public:
    static void displayHelp() {
           std::wcout << LR"(ps(1)                   CrossShell for UNIX Reference Manual                       ps(1)

    NAME
        ps - report active Windows processes

    SYNOPSIS
        ps [QUALIFIERS] [OUTPUT-OPTIONS]

    DESCRIPTION
        Reports active processes using familiar UNIX-style qualifiers and selectable
        output columns. Structured output is written to standard output and is safe
        to pipe to jq, ConvertFrom-Csv, Select-String, or another command.

    OPTIONS
        -e, -A, -a
            Select all active processes.

        -f
            Use the full format: USER, PID, PPID, STIME, TIME, COMMAND.

        -l
            Use the long format: STAT, PRI, THREADS, PID, PPID, VSZ, MEM, TIME,
            COMMAND.

        -p, -q, --pid PIDS
            Filter by a comma-delimited PID list, such as 1042,4088.

        -u, --user USER
            Filter by process owner or account username.

        -o, --format COLUMNS
            Select a comma-delimited list of output columns.

        --output FORMAT
            Select table, csv, json, or xml output. The default is table.

        --table
            Shortcut for --output table.

        --json
            Shortcut for --output json.

        --xml
            Shortcut for --output xml.

        --csv
            Shortcut for --output csv.

    AVAILABLE COLUMNS
        user, owner
            Process owner or user account name.

        pid
            Process identifier.

        ppid
            Parent process identifier.

        mem, rss, pmem
            Resident memory or working set.

        vsz, vsize
            Virtual memory size or pagefile usage.

        cpu, time, cputime
            Total accumulated CPU execution time.

        threads, thcnt
            Number of active threads.

        pri, priority
            Base priority class.

        stat, state
            Execution status.

        stime, start
            Process creation start time.

        comm, cmd, command
            Executable process name.

    DEFAULT LAYOUT
        USER, PID, MEM, CPU, THREADS, COMMAND

    EXAMPLES
        ps
            Display processes using the default layout.

        ps -ef
            Display all active processes in full format.

        ps -u Alice --json
            Emit Alice's processes as JSON.

        ps -p 1204,4096 -o pid,user,mem,cpu,command
            Display selected columns for two process IDs.

        ps -o user,pid,threads,mem --csv
            Emit selected process data as CSV.

        ps --json | jq '.[].command'
            Extract command names from JSON output.

        ps --csv | ConvertFrom-Csv | Where-Object { $_.USER -match 'SYSTEM' }
            Filter CSV output in PowerShell.

    CrossShell for UNIX                                                          ps(1)
    )";
    }

    static int run(int argc, wchar_t* argv[]) {
        PsConfig config = CommandLineParser::parse(argc, argv);

        if (config.showHelp) {
            displayHelp();
            return 0;
        }
        if (config.parseError) {
            std::wcerr << L"ps: unsupported output format (expected table, csv, json, or xml)\n";
            return 2;
        }

        // 1. Gather active processes
        auto all = ProcessCollector::collect();
        std::vector<ProcessRecord> filtered;

        // 2. Filter records
        for (const auto& proc : all) {
            if (!config.pidFilter.empty() && config.pidFilter.find(proc.pid) == config.pidFilter.end()) {
                continue;
            }
            if (!config.userFilter.empty()) {
                std::wstring userLower = proc.user;
                std::wstring filterLower = config.userFilter;
                std::transform(userLower.begin(), userLower.end(), userLower.begin(), ::towlower);
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::towlower);
                if (userLower.find(filterLower) == std::wstring::npos) {
                    continue;
                }
            }
            filtered.push_back(proc);
        }

        // 3. Resolve active column format
        std::vector<std::wstring> columns;
        if (!config.customColumns.empty()) {
            columns = config.customColumns;
        } else if (config.longFormat) {
            columns = { L"stat", L"pri", L"threads", L"pid", L"ppid", L"vsz", L"mem", L"cpu", L"command" };
        } else if (config.fullFormat) {
            columns = { L"user", L"pid", L"ppid", L"stime", L"cpu", L"command" };
        } else {
            // Default layout required: owner, PID, memory, CPU, threads, command
            columns = { L"user", L"pid", L"mem", L"cpu", L"threads", L"command" };
        }

        // 4. Dispatch Formatter Strategy
        std::unique_ptr<IOutputFormatter> formatter;
        switch (config.mode) {
            case OutputMode::CSV:  formatter = std::make_unique<CsvFormatter>(); break;
            case OutputMode::JSON: formatter = std::make_unique<JsonFormatter>(); break;
            case OutputMode::XML:  formatter = std::make_unique<XmlFormatter>(); break;
            case OutputMode::TABLE:
            default:               formatter = std::make_unique<TableFormatter>(); break;
        }

        formatter->render(filtered, columns);
        std::wcout.flush();
        return 0;
    }
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    return PsApplication::run(argc, argv);
}