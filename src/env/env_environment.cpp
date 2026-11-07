#include "env_environment.hpp"

#include <windows.h>

#include <limits>
#include <memory>

bool EnvironmentManager::CaseInsensitiveLess::operator()(
    const std::wstring& left, const std::wstring& right) const {
    const int comparison = CompareStringOrdinal(
        left.c_str(), -1, right.c_str(), -1, TRUE);
    if (comparison == CSTR_LESS_THAN) return true;
    if (comparison == CSTR_GREATER_THAN || comparison == CSTR_EQUAL) {
        return false;
    }
    return left < right;
}

bool EnvironmentManager::LoadSystemEnvironment(std::wstring& error) {
    LPWCH rawBlock = GetEnvironmentStringsW();
    if (!rawBlock) {
        error = L"cannot read the current environment (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }

    struct EnvironmentBlockDeleter {
        void operator()(wchar_t* block) const {
            if (block) FreeEnvironmentStringsW(block);
        }
    };
    std::unique_ptr<wchar_t, EnvironmentBlockDeleter> block(rawBlock);
    values_.clear();

    for (const wchar_t* current = rawBlock; *current;) {
        const std::wstring entry(current);
        current += entry.size() + 1;
        if (entry.empty() || entry.front() == L'=') continue;

        const std::size_t separator = entry.find(L'=');
        if (separator == std::wstring::npos || separator == 0) continue;
        values_[entry.substr(0, separator)] = entry.substr(separator + 1);
    }
    return true;
}

bool EnvironmentManager::Set(const std::wstring& name,
                             const std::wstring& value,
                             std::wstring& error) {
    if (!IsValidName(name)) {
        error = L"environment variable names must be non-empty and cannot "
                L"contain '=' or NUL.";
        return false;
    }
    values_[name] = value;
    return true;
}

bool EnvironmentManager::Unset(const std::wstring& name,
                               std::wstring& error) {
    if (!IsValidName(name)) {
        error = L"environment variable names must be non-empty and cannot "
                L"contain '=' or NUL.";
        return false;
    }
    values_.erase(name);
    return true;
}

bool EnvironmentManager::CreateEnvironmentBlock(
    std::vector<wchar_t>& block, std::wstring& error) const {
    constexpr std::size_t maximumLength = 32767;
    block.clear();

    for (const auto& [name, value] : values_) {
        if (name.size() > maximumLength - 2 ||
            value.size() > maximumLength - name.size() - 2) {
            error = L"the child environment exceeds the Windows size limit.";
            return false;
        }
        const std::size_t extra = name.size() + value.size() + 2;
        if (extra >= maximumLength ||
            block.size() > maximumLength - extra - 1) {
            error = L"the child environment exceeds the Windows size limit.";
            return false;
        }
        block.insert(block.end(), name.begin(), name.end());
        block.push_back(L'=');
        block.insert(block.end(), value.begin(), value.end());
        block.push_back(L'\0');
    }

    if (block.empty()) block.push_back(L'\0');
    block.push_back(L'\0');
    return true;
}

const EnvironmentManager::Map& EnvironmentManager::Values() const {
    return values_;
}

bool EnvironmentManager::IsValidName(const std::wstring& name) {
    return !name.empty() &&
        name.find(L'=') == std::wstring::npos &&
        name.find(L'\0') == std::wstring::npos;
}
