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

#include "event_reader.hpp"
#include "xml_helper.hpp"
#include <winevt.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cwctype>

namespace dmesg {

void EventLogReader::printKernelEvents(const DmesgOptions& options, bool colorsEnabled) {
    LPCWSTR channelPath = L"System";
    LPCWSTR xpathQuery = L"*";

    EVT_HANDLE hResults = EvtQuery(
        NULL,
        channelPath,
        xpathQuery,
        EvtQueryChannelPath | EvtQueryReverseDirection
    );

    if (NULL == hResults) {
        std::cerr << "EvtQuery failed with error: " << GetLastError() << std::endl;
        return;
    }

    EVT_HANDLE hEvent = NULL;
    DWORD dwReturned = 0;
    const DWORD maxEventsToShow = static_cast<DWORD>(options.maxEventsToShow);
    const DWORD maxEventsToScan = static_cast<DWORD>(options.maxEventsToScan);
    DWORD scannedCount = 0;
    DWORD shownCount = 0;

    auto color = [colorsEnabled](const wchar_t* c) -> const wchar_t* {
        return colorsEnabled ? c : L"";
    };

    std::wcout << color(AnsiColor::CLR_CYAN) << L"--- dmesg (Kernel/Driver Logs) ---" << color(AnsiColor::CLR_RESET) << L"\n\n";

    while (shownCount < maxEventsToShow && scannedCount < maxEventsToScan &&
           EvtNext(hResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
        DWORD dwBufferSize = 0;
        DWORD dwBufferUsed = 0;
        DWORD dwPropertyCount = 0;

        EvtRender(NULL, hEvent, EvtRenderEventXml, 0, NULL, &dwBufferUsed, &dwPropertyCount);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            dwBufferSize = dwBufferUsed;
            std::vector<wchar_t> buffer(dwBufferSize / sizeof(wchar_t));

            if (EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                std::wstring xml = buffer.data();
                const bool isKernel = xml.find(L"Provider Name='Microsoft-Windows-Kernel") != std::wstring::npos;
                const bool isDriver = xml.find(L"Provider Name='Microsoft-Windows-Driver") != std::wstring::npos;
                const bool isScm = xml.find(L"Provider Name='Service Control Manager'") != std::wstring::npos;
                const bool providerGroupMatch =
                    (options.includeKernel && isKernel) ||
                    (options.includeDriver && isDriver) ||
                    (options.includeScm && isScm);

                if (providerGroupMatch) {
                    const std::wstring provider = XmlHelper::extractAttribute(xml, L"<Provider", L"Name");
                    const std::wstring eventId = XmlHelper::extractTagValue(xml, L"EventID");
                    const std::wstring level = XmlHelper::extractTagValue(xml, L"Level");
                    const std::wstring created = XmlHelper::extractAttribute(xml, L"<TimeCreated", L"SystemTime");
                    const std::wstring details = XmlHelper::extractFirstDataLine(xml);

                    int eventLevel = 5;
                    try {
                        if (!level.empty()) {
                            eventLevel = std::stoi(level);
                        }
                    } catch (...) {
                        eventLevel = 5;
                    }
                    if (eventLevel > options.minLevel) {
                        EvtClose(hEvent);
                        scannedCount++;
                        hEvent = NULL;
                        continue;
                    }

                    if (!options.providerContains.empty()) {
                        std::wstring providerLower = provider;
                        std::transform(providerLower.begin(), providerLower.end(), providerLower.begin(),
                            [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
                        if (providerLower.find(options.providerContains) == std::wstring::npos) {
                            EvtClose(hEvent);
                            scannedCount++;
                            hEvent = NULL;
                            continue;
                        }
                    }

                    const wchar_t* levelColor = AnsiColor::levelColor(level);
                    const wchar_t* providerColor = AnsiColor::providerColor(provider);

                    std::wcout << color(AnsiColor::CLR_DIM) << L"[" << XmlHelper::formatSystemTime(created) << L"] " << color(AnsiColor::CLR_RESET)
                               << color(levelColor) << L"[" << XmlHelper::levelToText(level) << L"] " << color(AnsiColor::CLR_RESET)
                               << color(providerColor) << provider << color(AnsiColor::CLR_RESET)
                               << color(AnsiColor::CLR_DIM) << L" (EventID " << eventId << L")" << color(AnsiColor::CLR_RESET) << L"\n";
                    if (!details.empty()) {
                        std::wcout << color(AnsiColor::CLR_DIM) << L"  " << details << color(AnsiColor::CLR_RESET) << L"\n";
                    }
                    std::wcout << color(AnsiColor::CLR_DIM) << L"----------------------------------------" << color(AnsiColor::CLR_RESET) << L"\n";
                    shownCount++;
                }
            }
        }

        EvtClose(hEvent);
        scannedCount++;
        hEvent = NULL;
    }

    if (shownCount == 0) {
        std::cout << "No kernel/driver events matched recent System log entries." << std::endl;
    }

    EvtClose(hResults);
}

} // namespace dmesg
