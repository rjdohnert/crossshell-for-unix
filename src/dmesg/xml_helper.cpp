/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "xml_helper.hpp"
#include <vector>
#include <algorithm>

namespace dmesg {

std::wstring XmlHelper::extractAttribute(const std::wstring& xml, const std::wstring& anchor, const std::wstring& attribute) {
    const size_t anchorPos = xml.find(anchor);
    if (anchorPos == std::wstring::npos) return L"";

    const std::wstring token = attribute + L"='";
    const size_t attrPos = xml.find(token, anchorPos);
    if (attrPos == std::wstring::npos) return L"";

    const size_t valueStart = attrPos + token.size();
    const size_t valueEnd = xml.find(L"'", valueStart);
    if (valueEnd == std::wstring::npos) return L"";

    return xml.substr(valueStart, valueEnd - valueStart);
}

std::wstring XmlHelper::extractTagValue(const std::wstring& xml, const std::wstring& tag) {
    const std::wstring openTag = L"<" + tag;
    const size_t openPos = xml.find(openTag);
    if (openPos == std::wstring::npos) return L"";

    const size_t contentStart = xml.find(L">", openPos);
    if (contentStart == std::wstring::npos) return L"";

    const std::wstring closeTag = L"</" + tag + L">";
    const size_t contentEnd = xml.find(closeTag, contentStart + 1);
    if (contentEnd == std::wstring::npos) return L"";

    return xml.substr(contentStart + 1, contentEnd - (contentStart + 1));
}

std::wstring XmlHelper::extractFirstDataLine(const std::wstring& xml) {
    std::vector<std::wstring> parts;
    size_t cursor = 0;

    while (parts.size() < 3) {
        const size_t dataStart = xml.find(L"<Data", cursor);
        if (dataStart == std::wstring::npos) break;

        const size_t namePos = xml.find(L"Name='", dataStart);
        if (namePos == std::wstring::npos) break;
        const size_t nameStart = namePos + 6;
        const size_t nameEnd = xml.find(L"'", nameStart);
        if (nameEnd == std::wstring::npos) break;

        const size_t valueStart = xml.find(L">", nameEnd);
        if (valueStart == std::wstring::npos) break;
        const size_t valueEnd = xml.find(L"</Data>", valueStart + 1);
        if (valueEnd == std::wstring::npos) break;

        std::wstring key = xml.substr(nameStart, nameEnd - nameStart);
        std::wstring value = xml.substr(valueStart + 1, valueEnd - (valueStart + 1));
        if (!value.empty()) {
            parts.push_back(key + L"=" + value);
        }

        cursor = valueEnd + 7;
    }

    if (parts.empty()) return L"";

    std::wstring joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) joined += L" | ";
        joined += parts[i];
    }
    return joined;
}

std::wstring XmlHelper::formatSystemTime(std::wstring systemTime) {
    std::replace(systemTime.begin(), systemTime.end(), L'T', L' ');
    if (!systemTime.empty() && systemTime.back() == L'Z') {
        systemTime.pop_back();
    }
    return systemTime;
}

std::wstring XmlHelper::levelToText(const std::wstring& level) {
    if (level == L"1") return L"Critical";
    if (level == L"2") return L"Error";
    if (level == L"3") return L"Warning";
    if (level == L"4") return L"Info";
    if (level == L"5") return L"Verbose";
    return L"Unknown";
}

} // namespace dmesg
