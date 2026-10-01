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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cwctype>
#include <set>

using namespace std;

// --- Production RAII Wrapper for Registry Keys ---
class ScopedHKey {
    HKEY m_hKey;
public:
    explicit ScopedHKey(HKEY hKey = nullptr) : m_hKey(hKey) {}
    ~ScopedHKey() {
        if (m_hKey) RegCloseKey(m_hKey);
    }

    HKEY get() const { return m_hKey; }
    HKEY* receive() { return &m_hKey; }
    bool isValid() const { return m_hKey != nullptr; }

    // Prevent Copying
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;

    // Allow Moving
    ScopedHKey(ScopedHKey&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = nullptr;
    }
    ScopedHKey& operator=(ScopedHKey&& other) noexcept {
        if (this != &other) {
            if (m_hKey) RegCloseKey(m_hKey);
            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }
};

// --- Data Structures ---
struct SoftwareItem {
    wstring name;            // DisplayName / Product Name
    wstring revision;        // DisplayVersion
    wstring vendor;          // Publisher
    wstring installDate;     // InstallDate
    wstring location;        // InstallLocation
    wstring arch;            // x64, x86, or User
    bool isSystemComponent = false;
};

// ============================================================================
// 1. REGISTRY READER & SYSTEM INFO
// ============================================================================

class RegistryReader {
public:
    static wstring ReadString(HKEY hKey, const wchar_t* valueName) {
        DWORD dataType = 0;
        DWORD dataSize = 0;

        if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, nullptr, &dataSize) != ERROR_SUCCESS || dataSize == 0) {
            return L"";
        }

        vector<wchar_t> buffer((dataSize / sizeof(wchar_t)) + 1, L'\0');
        if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, 
                             reinterpret_cast<BYTE*>(buffer.data()), &dataSize) == ERROR_SUCCESS) {
            if (dataType == REG_SZ || dataType == REG_EXPAND_SZ) {
                return wstring(buffer.data());
            }
        }
        return L"";
    }

    static DWORD ReadDword(HKEY hKey, const wchar_t* valueName) {
        DWORD val = 0;
        DWORD size = sizeof(DWORD);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, 
                             reinterpret_cast<BYTE*>(&val), &size) == ERROR_SUCCESS) {
            if (type == REG_DWORD) return val;
        }
        return 0;
    }

    static void EnumerateHive(HKEY hRoot, const wchar_t* subkeyPath, REGSAM samDesired, 
                              const wstring& archLabel, vector<SoftwareItem>& items) {
        ScopedHKey hKey;
        if (RegOpenKeyExW(hRoot, subkeyPath, 0, KEY_READ | samDesired, hKey.receive()) != ERROR_SUCCESS) {
            return;
        }

        DWORD index = 0;
        wchar_t keyName[256];
        DWORD keyNameSize = 256;

        while (RegEnumKeyExW(hKey.get(), index++, keyName, &keyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            keyNameSize = 256;

            ScopedHKey hSubKey;
            if (RegOpenKeyExW(hKey.get(), keyName, 0, KEY_READ | samDesired, hSubKey.receive()) == ERROR_SUCCESS) {
                wstring name = ReadString(hSubKey.get(), L"DisplayName");
                if (name.empty()) continue;

                SoftwareItem item;
                item.name = name;
                item.revision = ReadString(hSubKey.get(), L"DisplayVersion");
                if (item.revision.empty()) item.revision = L"N/A";

                item.vendor = ReadString(hSubKey.get(), L"Publisher");
                if (item.vendor.empty()) item.vendor = L"Unknown";

                item.installDate = ReadString(hSubKey.get(), L"InstallDate");
                if (item.installDate.empty()) item.installDate = L"N/A";

                item.location = ReadString(hSubKey.get(), L"InstallLocation");
                if (item.location.empty()) item.location = L"N/A";

                item.arch = archLabel;
                item.isSystemComponent = (ReadDword(hSubKey.get(), L"SystemComponent") == 1);

                items.push_back(item);
            }
        }
    }
};

class SystemInfo {
public:
    static wstring GetHostNameString() {
        wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(buf, &size)) {
            return wstring(buf);
        }
        return L"localhost";
    }

    static wstring ToUpper(wstring str) {
        transform(str.begin(), str.end(), str.begin(), ::towupper);
        return str;
    }
};

// ============================================================================
// 2. SOFTWARE COLLECTOR & INVENTORY ENGINE
// ============================================================================

class SoftwareCollector {
public:
    static vector<SoftwareItem> CollectInstalledSoftware() {
        vector<SoftwareItem> items;
        const wchar_t* uninstallPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

        // 1. 64-bit System Applications (HKLM x64)
        RegistryReader::EnumerateHive(HKEY_LOCAL_MACHINE, uninstallPath, KEY_WOW64_64KEY, L"x64", items);

        // 2. 32-bit System Applications (HKLM WOW64)
        RegistryReader::EnumerateHive(HKEY_LOCAL_MACHINE, uninstallPath, KEY_WOW64_32KEY, L"x86", items);

        // 3. User-installed Applications (HKCU)
        RegistryReader::EnumerateHive(HKEY_CURRENT_USER, uninstallPath, 0, L"User", items);

        sort(items.begin(), items.end(), [](const SoftwareItem& a, const SoftwareItem& b) {
            return SystemInfo::ToUpper(a.name) < SystemInfo::ToUpper(b.name);
        });

        auto last = unique(items.begin(), items.end(), [](const SoftwareItem& a, const SoftwareItem& b) {
            return a.name == b.name && a.revision == b.revision && a.arch == b.arch;
        });
        items.erase(last, items.end());

        return items;
    }

    static vector<SoftwareItem> Filter(const vector<SoftwareItem>& allSoftware, const vector<wstring>& searchPatterns, bool showSystemComponents) {
        vector<SoftwareItem> filtered;
        vector<wstring> upperPatterns;
        upperPatterns.reserve(searchPatterns.size());
        for (const auto& pattern : searchPatterns) {
            upperPatterns.push_back(SystemInfo::ToUpper(pattern));
        }

        for (const auto& item : allSoftware) {
            if (!showSystemComponents && item.isSystemComponent) {
                continue;
            }

            if (!upperPatterns.empty()) {
                bool matches = false;
                const wstring upperName = SystemInfo::ToUpper(item.name);
                const wstring upperVendor = SystemInfo::ToUpper(item.vendor);
                for (const auto& pattern : upperPatterns) {
                    if (upperName.find(pattern) != wstring::npos ||
                        upperVendor.find(pattern) != wstring::npos) {
                        matches = true;
                        break;
                    }
                }
                if (!matches) continue;
            }

            filtered.push_back(item);
        }
        return filtered;
    }
};

// ============================================================================
// 3. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void EmitHeader(const wstring& hostName) {
        wcout << L"#\n"
              << L"# Target Selection Spec:\n"
              << L"#   Host: " << hostName << L"\n"
              << L"#\n";
    }

    static void EmitVerbose(const vector<SoftwareItem>& items) {
        wcout << left 
              << setw(35) << L"# Name"
              << setw(16) << L"Revision"
              << setw(22) << L"Vendor"
              << setw(12) << L"Date"
              << setw(8)  << L"Arch"
              << L"Location\n";
        wcout << wstring(115, L'=') << L"\n";

        for (const auto& item : items) {
            wcout << left
                  << setw(35) << item.name.substr(0, 34)
                  << setw(16) << item.revision.substr(0, 15)
                  << setw(22) << item.vendor.substr(0, 21)
                  << setw(12) << item.installDate.substr(0, 11)
                  << setw(8)  << item.arch
                  << item.location << L"\n";
        }
    }

    static int EmitAttribute(const vector<SoftwareItem>& items, const wstring& attribute) {
        if (attribute == L"TITLE") {
            wcout << L"# Name\n" << wstring(60, L'=') << L"\n";
            for (const auto& item : items) wcout << item.name << L"\n";
        } else if (attribute == L"REVISION" || attribute == L"VERSION") {
            wcout << left << setw(45) << L"# Name" << L"Revision\n" << wstring(65, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.revision << L"\n";
            }
        } else if (attribute == L"VENDOR" || attribute == L"PUBLISHER") {
            wcout << left << setw(45) << L"# Name" << L"Vendor\n" << wstring(75, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.vendor << L"\n";
            }
        } else if (attribute == L"DATE") {
            wcout << left << setw(45) << L"# Name" << L"Install Date\n" << wstring(65, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.installDate << L"\n";
            }
        } else if (attribute == L"ARCH" || attribute == L"ARCHITECTURE") {
            wcout << left << setw(45) << L"# Name" << L"Architecture\n" << wstring(60, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.arch << L"\n";
            }
        } else if (attribute == L"LOCATION" || attribute == L"PATH") {
            wcout << left << setw(45) << L"# Name" << L"Location\n" << wstring(85, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.location << L"\n";
            }
        } else {
            wcerr << L"Unknown attribute: " << attribute << L"\nUse -h for help.\n";
            return 1;
        }
        return 0;
    }

    static void EmitVendorGrouped(const vector<SoftwareItem>& items) {
        vector<SoftwareItem> vendorView = items;
        sort(vendorView.begin(), vendorView.end(), [](const SoftwareItem& a, const SoftwareItem& b) {
            const wstring va = SystemInfo::ToUpper(a.vendor);
            const wstring vb = SystemInfo::ToUpper(b.vendor);
            if (va != vb) return va < vb;
            return SystemInfo::ToUpper(a.name) < SystemInfo::ToUpper(b.name);
        });

        wstring currentVendor;
        for (const auto& item : vendorView) {
            if (currentVendor != item.vendor) {
                currentVendor = item.vendor;
                wcout << L"\n" << currentVendor << L"\n";
                wcout << wstring(currentVendor.length(), L'-') << L"\n";
            }
            wcout << left
                  << setw(42) << item.name.substr(0, 41)
                  << setw(18) << item.revision.substr(0, 17)
                  << item.arch << L"\n";
        }
    }

    static void EmitDefault(const vector<SoftwareItem>& items) {
        wcout << left 
              << setw(42) << L"# Name"
              << setw(18) << L"Revision"
              << L"Vendor / Title\n";
        wcout << wstring(85, L'=') << L"\n";

        for (const auto& item : items) {
            wcout << left
                  << setw(42) << item.name.substr(0, 41)
                  << setw(18) << item.revision.substr(0, 17)
                  << item.vendor << L"\n";
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct SwlistOptions {
    wstring level = L"PRODUCT";
    wstring attribute = L"ALL";
    bool verbose = false;
    bool showSystemComponents = false;
    bool showHelp = false;
    vector<wstring> searchPatterns;
    int output_format = 0;
    wstring pipe_command;
};

class OptionParser {
public:
    static bool IsValidLevel(const wstring& level) {
        static const set<wstring> kValidLevels = {
            L"PRODUCT", L"BUNDLE", L"VENDOR", L"ALL"
        };
        return kValidLevels.find(level) != kValidLevels.end();
    }

    static bool IsValidAttribute(const wstring& attribute) {
        static const set<wstring> kValidAttributes = {
            L"ALL", L"TITLE", L"REVISION", L"VERSION", L"VENDOR", L"PUBLISHER",
            L"DATE", L"LOCATION", L"PATH", L"ARCH", L"ARCHITECTURE"
        };
        return kValidAttributes.find(attribute) != kValidAttributes.end();
    }

    static void PrintHelp(const wchar_t* progName) {
        wcout << L"Software Distributor Inspector v9.2.0\n"
              << L"Usage: " << progName << L" [-l level] [-a attribute] [-v] [pattern ...]\n\n"
              << L"OPTIONS:\n"
              << L"  -l level       Specify the detail level of software listing:\n"
              << L"                   product   List top-level installed products (default).\n"
              << L"                   bundle    Alias for product view for compatibility.\n"
              << L"                   vendor    Group products by vendor/publisher.\n"
              << L"                   all       Display all software including system components.\n"
              << L"  -a attribute   Display a specific software attribute:\n"
              << L"                   title     Display product name only.\n"
              << L"                   revision  Display product name and version.\n"
              << L"                   vendor    Display product name and vendor.\n"
              << L"                   date      Display product name and install date.\n"
              << L"                   location  Display product name and install path.\n"
              << L"                   arch      Display product name and architecture (x64/x86/User).\n"
              << L"  -v, -f         Verbose full view showing revision, vendor, date, arch, and path.\n"
              << L"  -h, /?         Display this comprehensive help documentation.\n\n"
              << L"OPERANDS:\n"
              << L"  pattern        Case-insensitive filter string matching software names or vendors.\n\n"
              << L"EXAMPLES:\n"
              << L"  " << progName << L"\n"
              << L"  " << progName << L" -v\n"
              << L"  " << progName << L" -a revision\n"
              << L"  " << progName << L" -a vendor Python\n"
              << L"  " << progName << L" -l vendor Microsoft\n"
              << L"  " << progName << L" Microsoft\n";
        wcout << L"  --json, --csv, --table  Select output format\n"
              << L"  --pipe COMMAND          Send output through COMMAND\n";
    }

    bool Parse(int argc, wchar_t* argv[], SwlistOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"-v" || arg == L"-f" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-l") {
                if (i + 1 < argc) {
                    opts.level = SystemInfo::ToUpper(argv[++i]);
                } else {
                    wcerr << L"Error: -l option requires a level argument.\n";
                    return false;
                }
            } else if (arg == L"-a") {
                if (i + 1 < argc) {
                    opts.attribute = SystemInfo::ToUpper(argv[++i]);
                } else {
                    wcerr << L"Error: -a option requires an attribute argument.\n";
                    return false;
                }
            } else if (arg == L"--json") {
                opts.output_format = 1;
            } else if (arg == L"--csv") {
                opts.output_format = 2;
            } else if (arg == L"--table") {
                opts.output_format = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipe_command = argv[++i];
            } else if (arg.length() > 1 && (arg[0] == L'-' || arg[0] == L'/')) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'v' || c == L'f') opts.verbose = true;
                    else if (c == L'l') {
                        if (j + 1 < arg.length()) {
                            opts.level = SystemInfo::ToUpper(arg.substr(j + 1));
                            break;
                        } else if (i + 1 < argc) {
                            opts.level = SystemInfo::ToUpper(argv[++i]);
                            break;
                        } else {
                            wcerr << L"Error: -l option requires a level argument.\n";
                            return false;
                        }
                    } else if (c == L'a') {
                        if (j + 1 < arg.length()) {
                            opts.attribute = SystemInfo::ToUpper(arg.substr(j + 1));
                            break;
                        } else if (i + 1 < argc) {
                            opts.attribute = SystemInfo::ToUpper(argv[++i]);
                            break;
                        } else {
                            wcerr << L"Error: -a option requires an attribute argument.\n";
                            return false;
                        }
                    } else {
                        wcerr << L"Unknown flag option: -" << c << L"\nUse -h for help.\n";
                        return false;
                    }
                }
            } else {
                opts.searchPatterns.push_back(arg);
            }
        }

        if (!IsValidLevel(opts.level)) {
            wcerr << L"Unknown level: " << opts.level << L"\nUse -h for help.\n";
            return false;
        }

        if (!IsValidAttribute(opts.attribute)) {
            wcerr << L"Unknown attribute: " << opts.attribute << L"\nUse -h for help.\n";
            return false;
        }

        if (opts.level == L"BUNDLE") {
            opts.level = L"PRODUCT";
        }

        if (opts.level == L"ALL") {
            opts.showSystemComponents = true;
        }

        return true;
    }
};

class SwlistApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        SwlistOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintHelp(argv[0]);
            return 0;
        }

        vector<SoftwareItem> allSoftware = SoftwareCollector::CollectInstalledSoftware();
        vector<SoftwareItem> filteredSoftware = SoftwareCollector::Filter(allSoftware, opts.searchPatterns, opts.showSystemComponents);

        wstring hostName = SystemInfo::GetHostNameString();
        OutputFormatter::EmitHeader(hostName);

        if (filteredSoftware.empty()) {
            wcout << L"# No software products matched the specified selection.\n";
            return 0;
        }

        if (opts.verbose) {
            OutputFormatter::EmitVerbose(filteredSoftware);
            return 0;
        }

        if (opts.attribute != L"ALL") {
            return OutputFormatter::EmitAttribute(filteredSoftware, opts.attribute);
        }

        if (opts.level == L"VENDOR") {
            OutputFormatter::EmitVendorGrouped(filteredSoftware);
            return 0;
        }

        OutputFormatter::EmitDefault(filteredSoftware);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SwlistApplication app;
    return app.Run(argc, argv);
}