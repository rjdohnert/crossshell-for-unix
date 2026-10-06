#include "unxz_backend_config.hpp"
#include "unxz_backend_finder.hpp"

std::wstring UnxzBackendFinder::toLower(const std::wstring& s) {
        std::wstring out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return out;
    }

std::wstring UnxzBackendFinder::normalizePath(const std::wstring& p) {
        std::error_code ec;
        fs::path fp(p);
        fs::path absPath = fs::absolute(fp, ec);
        if (!ec) {
            fs::path canon = fs::weakly_canonical(absPath, ec);
            if (!ec) return toLower(canon.wstring());
            return toLower(absPath.wstring());
        }
        return toLower(fp.wstring());
    }

std::wstring UnxzBackendFinder::findInPath(const std::wstring& exe) {
        wchar_t buf[MAX_PATH];
        DWORD len = SearchPathW(nullptr, exe.c_str(), nullptr, MAX_PATH, buf, nullptr);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
        return L"";
    }

std::wstring UnxzBackendFinder::selfExePath() {
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
        return L"";
    }

UnxzBackendConfig UnxzBackendFinder::discoverBackend() {
        std::wstring self = normalizePath(selfExePath());

        // Check for native xz.exe
        std::wstring xzPath = findInPath(L"xz.exe");
        if (!xzPath.empty() && normalizePath(xzPath) != self) {
            return { xzPath, { L"-d" } };
        }

        // Check 7-Zip in PATH and standard locations
        std::vector<std::wstring> sevenZipCandidates = {
            L"7z.exe", L"7za.exe",
            L"C:\\Program Files\\7-Zip\\7z.exe",
            L"C:\\Program Files (x86)\\7-Zip\\7z.exe"
        };

        for (const auto& cand : sevenZipCandidates) {
            std::wstring found = findInPath(cand);
            if (found.empty()) {
                std::error_code ec;
                if (fs::exists(cand, ec)) found = cand;
            }
            if (!found.empty() && normalizePath(found) != self) {
                return { found, { L"x" } };
            }
        }

        return {};
    }
