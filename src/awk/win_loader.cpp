/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 * CrossShell for UNIX
 */

#define _CRT_SECURE_NO_WARNINGS
#include "win_loader.hpp"
#include "string_utils.hpp"
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef _WIN32
bool WindowsObjectLoader::LoadRegistry(const std::string& spec, std::string& output, std::string& error) {
    size_t slash = spec.find('\\');
    std::string rootName = slash == std::string::npos ? spec : spec.substr(0, slash);
    std::string subKey = slash == std::string::npos ? "" : spec.substr(slash + 1);
    HKEY root = nullptr;
    if (_stricmp(rootName.c_str(), "HKLM") == 0 || _stricmp(rootName.c_str(), "HKEY_LOCAL_MACHINE") == 0) root = HKEY_LOCAL_MACHINE;
    else if (_stricmp(rootName.c_str(), "HKCU") == 0 || _stricmp(rootName.c_str(), "HKEY_CURRENT_USER") == 0) root = HKEY_CURRENT_USER;
    else if (_stricmp(rootName.c_str(), "HKCR") == 0 || _stricmp(rootName.c_str(), "HKEY_CLASSES_ROOT") == 0) root = HKEY_CLASSES_ROOT;
    else if (_stricmp(rootName.c_str(), "HKU") == 0 || _stricmp(rootName.c_str(), "HKEY_USERS") == 0) root = HKEY_USERS;
    else { error = "invalid registry root '" + rootName + "'"; return false; }

    HKEY key = nullptr;
    LONG status = RegOpenKeyExA(root, subKey.c_str(), 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) { error = "cannot open registry key '" + spec + "'"; return false; }
    for (DWORD index = 0;; ++index) {
        wchar_t name[16384] = {};
        DWORD nameSize = static_cast<DWORD>(sizeof(name) / sizeof(name[0]));
        DWORD type = 0;
        BYTE data[65536] = {};
        DWORD dataSize = sizeof(data);
        status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) continue;
        std::string value;
        if (type == REG_DWORD && dataSize >= sizeof(DWORD)) value = std::to_string(*reinterpret_cast<DWORD*>(data));
        else if (type == REG_QWORD && dataSize >= sizeof(ULONGLONG)) value = std::to_string(*reinterpret_cast<ULONGLONG*>(data));
        else if (type == REG_SZ || type == REG_EXPAND_SZ) {
            size_t chars = dataSize / sizeof(wchar_t);
            std::wstring wide(reinterpret_cast<const wchar_t*>(data), chars);
            if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
            value = StringUtils::WideUtf8(wide.c_str());
        } else value.assign(reinterpret_cast<char*>(data), dataSize);
        output += "{\"name\":\"" + StringUtils::JsonEscape(StringUtils::WideUtf8(name)) + "\",\"value\":\"" + StringUtils::JsonEscape(value) + "\"}\n";
    }
    RegCloseKey(key);
    return true;
}

bool WindowsObjectLoader::LoadWmi(const std::string& spec, std::string& output, std::string& error) {
    size_t pipe = spec.find('|');
    std::string query = pipe == std::string::npos ? spec : spec.substr(0, pipe);
    std::wstring namespaceName = L"ROOT\\CIMV2";
    if (pipe != std::string::npos) {
        int size = MultiByteToWideChar(CP_UTF8, 0, spec.substr(pipe + 1).c_str(), -1, nullptr, 0);
        namespaceName.resize(size > 0 ? size - 1 : 0);
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, spec.substr(pipe + 1).c_str(), -1, namespaceName.data(), size);
    }
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool uninitialize = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) { error = "cannot initialize COM for WMI"; return false; }
    IWbemLocator* locator = nullptr; IWbemServices* services = nullptr; IEnumWbemClassObject* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
    if (SUCCEEDED(hr)) hr = locator->ConnectServer(_bstr_t(namespaceName.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    if (SUCCEEDED(hr)) hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    int querySize = MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), nullptr, 0);
    std::wstring wideQuery(querySize, L'\0');
    if (querySize > 0) MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), wideQuery.data(), querySize);
    if (SUCCEEDED(hr)) hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wideQuery.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* object = nullptr; ULONG returned = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) == S_OK && returned == 1) {
            SAFEARRAY* names = nullptr; hr = object->GetNames(nullptr, WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names);
            output += "{";
            if (SUCCEEDED(hr) && names) {
                LONG lower = 0, upper = -1;
                SafeArrayGetLBound(names, 1, &lower);
                SafeArrayGetUBound(names, 1, &upper);
                for (LONG i = lower; i <= upper; ++i) {
                    BSTR property = nullptr;
                    SafeArrayGetElement(names, &i, &property);
                    VARIANT value;
                    VariantInit(&value);
                    object->Get(property, 0, &value, nullptr, nullptr);
                    if (i > lower) output += ",";
                    std::string key = StringUtils::WideUtf8(property);
                    output += "\"" + StringUtils::JsonEscape(key) + "\":" + StringUtils::VariantJson(value);
                    VariantClear(&value);
                    SysFreeString(property);
                }
                SafeArrayDestroy(names);
            }
            output += "}\n";
            object->Release();
        }
    }
    if (enumerator) enumerator->Release();
    if (services) services->Release();
    if (locator) locator->Release();
    if (uninitialize) CoUninitialize();
    if (FAILED(hr)) { error = "WMI query failed"; return false; }
    return true;
}
#endif

bool WindowsObjectLoader::LoadObjectInput(const std::string& source, std::string& output, std::string& error) {
#ifdef _WIN32
    if (source.rfind("registry:", 0) == 0) return LoadRegistry(source.substr(9), output, error);
    return LoadWmi(source.substr(4), output, error);
#else
    error = "registry and WMI input require Windows"; return false;
#endif
}
