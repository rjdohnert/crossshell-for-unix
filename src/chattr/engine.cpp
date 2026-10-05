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

#include "engine.hpp"
#include <cwctype>
#include <iostream>

#ifdef _WIN32
AttrFlags FileAttributeEngine::ReadFlags(DWORD attrs) {
    AttrFlags flags;
    flags.archive = (attrs & FILE_ATTRIBUTE_ARCHIVE) != 0;
    flags.hidden = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
    flags.system = (attrs & FILE_ATTRIBUTE_SYSTEM) != 0;
    flags.readonly = (attrs & FILE_ATTRIBUTE_READONLY) != 0;
    flags.immutable = flags.readonly;
    return flags;
}

bool FileAttributeEngine::WriteFlags(const std::wstring& path, const AttrFlags& flags) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    if (flags.archive) attrs |= FILE_ATTRIBUTE_ARCHIVE;
    else attrs &= ~FILE_ATTRIBUTE_ARCHIVE;
    if (flags.hidden) attrs |= FILE_ATTRIBUTE_HIDDEN;
    else attrs &= ~FILE_ATTRIBUTE_HIDDEN;
    if (flags.system) attrs |= FILE_ATTRIBUTE_SYSTEM;
    else attrs &= ~FILE_ATTRIBUTE_SYSTEM;
    if (flags.readonly || flags.immutable) attrs |= FILE_ATTRIBUTE_READONLY;
    else attrs &= ~FILE_ATTRIBUTE_READONLY;

    return SetFileAttributesW(path.c_str(), attrs) != FALSE;
}
#else
bool FileAttributeEngine::WriteFlags(const std::wstring&, const AttrFlags&) {
    return false;
}
#endif

void FileAttributeEngine::ApplySpec(const std::wstring& spec, AttrFlags& flags) {
    if (spec.size() < 2) return;

    wchar_t op = spec[0];
    for (size_t i = 1; i < spec.size(); ++i) {
        wchar_t c = static_cast<wchar_t>(towlower(spec[i]));
        switch (c) {
            case L'a': flags.archive = (op == L'+'); break;
            case L'h': flags.hidden = (op == L'+'); break;
            case L's': flags.system = (op == L'+'); break;
            case L'i': flags.immutable = (op == L'+'); flags.readonly = (op == L'+'); break;
            default: break;
        }
    }
}

std::wstring FileAttributeEngine::FormatFlags(const AttrFlags& flags) {
    std::wstring out;
    out += (flags.archive ? L'a' : L'-');
    out += (flags.hidden ? L'h' : L'-');
    out += (flags.system ? L's' : L'-');
    out += (flags.immutable ? L'i' : L'-');
    return out;
}

bool AttributeTraverser::ProcessFile(const std::wstring& path, const std::vector<std::wstring>& specs, bool recursive, bool statusOnly, std::wostream& out, std::wostream& err) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        err << L"chattr: " << path << L": No such file or directory\n";
        return false;
    }

    bool ok = true;
    if (statusOnly) {
        AttrFlags flags = FileAttributeEngine::ReadFlags(attrs);
        out << FileAttributeEngine::FormatFlags(flags) << L" " << path << L"\n";
    } else {
        AttrFlags flags = FileAttributeEngine::ReadFlags(attrs);
        for (const auto& spec : specs) {
            FileAttributeEngine::ApplySpec(spec, flags);
        }
        if (!FileAttributeEngine::WriteFlags(path, flags)) {
            err << L"chattr: failed to set attributes on " << path << L"\n";
            ok = false;
        }
    }

    if (recursive && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring pattern = path;
        if (!pattern.empty() && pattern.back() != L'\\') {
            pattern.push_back(L'\\');
        }
        pattern.push_back(L'*');

        WIN32_FIND_DATAW findData = {};
        ScopedFindHandle hFind(FindFirstFileW(pattern.c_str(), &findData));
        if (hFind.IsValid()) {
            do {
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                    continue;
                }
                std::wstring child = path;
                if (!child.empty() && child.back() != L'\\') {
                    child.push_back(L'\\');
                }
                child += findData.cFileName;

                if (!ProcessFile(child, specs, recursive, statusOnly, out, err)) {
                    ok = false;
                }
            } while (FindNextFileW(hFind.Get(), &findData));
        }
    }

    return ok;
#else
    (void)path; (void)specs; (void)recursive; (void)statusOnly; (void)out; (void)err;
    return false;
#endif
}
