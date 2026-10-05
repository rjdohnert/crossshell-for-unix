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

#include "mode_parser.hpp"
#include <vector>

DWORD AccessMaskCalculator::OctalToAccessMask(int digit, bool isDirectory) {
    DWORD mask = 0;

#ifdef _WIN32
    if (digit & 4) {
        mask |= FILE_GENERIC_READ;
    }
    if (digit & 2) {
        mask |= FILE_GENERIC_WRITE | DELETE;
    }
    if (digit & 1) {
        mask |= FILE_GENERIC_EXECUTE;
    }

    if (isDirectory && (digit & 1)) {
        mask |= FILE_LIST_DIRECTORY | FILE_TRAVERSE;
    }
#else
    (void)digit;
    (void)isDirectory;
#endif

    return mask;
}

bool ModeParser::Parse(const std::wstring& modeStr, int& userMode, int& groupMode, int& otherMode, std::wostream& err) {
    userMode = 0;
    groupMode = 0;
    otherMode = 0;

    if (modeStr.length() >= 3 && modeStr.find_first_not_of(L"01234567") == std::wstring::npos) {
        std::wstring s = modeStr;
        if (s.length() == 4) s = s.substr(1);

        userMode = s[0] - L'0';
        groupMode = s[1] - L'0';
        otherMode = s[2] - L'0';
        return true;
    }

    std::wstring spec = modeStr;
    if (spec.empty()) {
        err << L"Error: Unsupported mode '" << modeStr << L"'\n";
        return false;
    }

    auto apply_permission = [](int& target, wchar_t ch, bool add, bool remove) {
        switch (ch) {
            case L'r': add ? (target |= 4) : (target &= ~4); break;
            case L'w': add ? (target |= 2) : (target &= ~2); break;
            case L'x': add ? (target |= 1) : (target &= ~1); break;
            case L'X': add ? (target |= 1) : (target &= ~1); break;
            default: break;
        }
    };

    auto apply_clause = [&](const std::wstring& clause, bool add, bool remove, bool set) -> bool {
        if (clause.empty()) return false;

        size_t opPos = std::wstring::npos;
        for (size_t i = 0; i < clause.size(); ++i) {
            if (clause[i] == L'+' || clause[i] == L'-' || clause[i] == L'=') {
                opPos = i;
                break;
            }
        }
        if (opPos == std::wstring::npos || opPos + 1 >= clause.size()) {
            if (!(set && opPos + 1 == clause.size())) {
                return false;
            }
        }

        std::wstring who = clause.substr(0, opPos);
        std::wstring perms = (opPos + 1 < clause.size()) ? clause.substr(opPos + 1) : L"";
        if (who.empty()) who = L"a";

        std::vector<int*> targets;
        if (who.find(L'u') != std::wstring::npos || who == L"a") {
            targets.push_back(&userMode);
        }
        if (who.find(L'g') != std::wstring::npos || who == L"a") {
            targets.push_back(&groupMode);
        }
        if (who.find(L'o') != std::wstring::npos || who == L"a") {
            targets.push_back(&otherMode);
        }
        if (targets.empty()) {
            return false;
        }

        for (int* target : targets) {
            if (set) {
                *target = 0;
            }
            for (wchar_t ch : perms) {
                if (add) {
                    apply_permission(*target, ch, true, false);
                } else if (remove) {
                    apply_permission(*target, ch, false, true);
                } else {
                    apply_permission(*target, ch, false, false);
                }
            }
        }
        return true;
    };

    std::vector<std::wstring> clauses;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t comma = spec.find(L',', start);
        if (comma == std::wstring::npos) {
            clauses.push_back(spec.substr(start));
            break;
        }
        clauses.push_back(spec.substr(start, comma - start));
        start = comma + 1;
    }

    bool parsed = false;
    for (const auto& clause : clauses) {
        if (clause.empty()) continue;
        bool add = false;
        bool remove = false;
        bool set = false;
        if (clause.find(L'+') != std::wstring::npos) {
            add = true;
        } else if (clause.find(L'-') != std::wstring::npos) {
            remove = true;
        } else if (clause.find(L'=') != std::wstring::npos) {
            set = true;
        } else {
            err << L"Error: Unsupported mode '" << modeStr << L"'\n";
            return false;
        }

        if (!apply_clause(clause, add, remove, set)) {
            err << L"Error: Unsupported mode '" << modeStr << L"'\n";
            return false;
        }
        parsed = true;
    }

    if (!parsed) {
        err << L"Error: Unsupported mode '" << modeStr << L"'\n";
        return false;
    }

    return true;
}
