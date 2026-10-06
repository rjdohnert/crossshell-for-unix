#include "registry_reader.hpp"
#include "software_collector.hpp"
#include "software_item.hpp"
#include "system_info.hpp"

vector<SoftwareItem> SoftwareCollector::CollectInstalledSoftware() {
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

vector<SoftwareItem> SoftwareCollector::Filter(const vector<SoftwareItem>& allSoftware, const vector<wstring>& searchPatterns, bool showSystemComponents) {
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
