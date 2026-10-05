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

#ifndef DLADM_MODELS_HPP
#define DLADM_MODELS_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <string>

namespace dladm {

enum class LinkClass {
    Phys,   // Physical NDIS miniport adapter
    Vnic,   // Hyper-V Virtual Ethernet Adapter / VNIC
    Aggr,   // NetLBFO Teaming / LACP Link Aggregation
    Vlan    // NDIS 802.1Q Tagged Interface
};

struct DataLinkInfo {
    std::string linkName;       // e.g., net0, net1, vnic0, aggr0
    std::string adapterName;    // Win32 Friendly Name
    LinkClass linkClass;        // Phys, Vnic, Aggr, Vlan
    UINT32 mtu = 1500;          // Maximum Transmission Unit
    std::string state;          // up, down, unknown
    std::string media;          // Ethernet, Wi-Fi, Loopback
    UINT64 speedBps = 0;        // Link speed in bits per second
    std::string duplex;         // full, half, unknown
    std::string macAddress;     // HH:HH:HH:HH:HH:HH
    std::string deviceName;     // NDIS Device Instance String
    std::string overLink = "--";// Underlying physical link for VNIC/VLAN/Aggr
    UINT16 vlanId = 0;          // VLAN ID if applicable
};

std::string WStringToUtf8(const std::wstring& value);
std::string FormatMacAddress(const BYTE* mac, DWORD length);
std::string FormatSpeed(UINT64 bps);
std::string TruncateString(const std::string& value, size_t maxLen);
std::string LinkClassToString(LinkClass lc);
bool IsSyntheticWindowsInterface(const std::string& adapterName, const std::string& friendlyName, ULONG ifType);

} // namespace dladm

#endif // DLADM_MODELS_HPP
