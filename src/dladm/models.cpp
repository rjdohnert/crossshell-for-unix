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

#include "models.hpp"
#include <winsock2.h>
#include <iphlpapi.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace dladm {

std::string WStringToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        out.data(),
        required,
        nullptr,
        nullptr);

    if (written <= 0) {
        return {};
    }

    return out;
}

std::string FormatMacAddress(const BYTE* mac, DWORD length) {
    if (!mac || length == 0) return "--";
    std::ostringstream oss;
    for (DWORD i = 0; i < length; ++i) {
        if (i > 0) oss << ":";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
    }
    return oss.str();
}

std::string FormatSpeed(UINT64 bps) {
    if (bps == 0) return "0";
    double mbps = static_cast<double>(bps) / 1000000.0;
    if (mbps >= 1000.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << (mbps / 1000.0) << "Gb";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << mbps << "Mb";
        return oss.str();
    }
}

std::string TruncateString(const std::string& value, size_t maxLen) {
    if (value.length() <= maxLen) {
        return value;
    }
    if (maxLen <= 3) {
        return value.substr(0, maxLen);
    }
    return value.substr(0, maxLen - 3) + "...";
}

std::string LinkClassToString(LinkClass lc) {
    switch (lc) {
        case LinkClass::Phys: return "phys";
        case LinkClass::Vnic: return "vnic";
        case LinkClass::Aggr: return "aggr";
        case LinkClass::Vlan: return "vlan";
        default: return "unknown";
    }
}

bool IsSyntheticWindowsInterface(const std::string& adapterName, const std::string& friendlyName, ULONG ifType) {
    if (adapterName.empty() && friendlyName.empty()) {
        return true;
    }

    std::string text = friendlyName;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const bool isHyperVVirtual = (text.find("vethernet") != std::string::npos || text.find("hyper-v") != std::string::npos);
    if (isHyperVVirtual) {
        return false;
    }

    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }

    if (ifType == IF_TYPE_TUNNEL) {
        return true;
    }

    const bool synthetic =
        text.find("wfp") != std::string::npos ||
        text.find("qos packet scheduler") != std::string::npos ||
        text.find("teredo") != std::string::npos ||
        text.find("6to4") != std::string::npos ||
        text.find("ip-https") != std::string::npos ||
        text.find("isatap") != std::string::npos ||
        text.find("pseudo-interface") != std::string::npos ||
        text.find("virtual wifi filter driver") != std::string::npos ||
        text.find("native mac layer lightweight filter") != std::string::npos ||
        text.find("802.3 mac layer lightweight filter") != std::string::npos ||
        text.find("kernel debugger") != std::string::npos ||
        text.find("vpn") != std::string::npos ||
        text.find("ras") != std::string::npos ||
        text.find("microsoft") != std::string::npos;

    return synthetic;
}

} // namespace dladm
