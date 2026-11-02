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

#include "link_manager.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <sstream>

namespace dladm {

std::vector<DataLinkInfo> LinkManager::DiscoverDataLinks() {
    std::vector<DataLinkInfo> links;

    ULONG family = AF_UNSPEC;
    ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;
    ULONG bufSize = 15360; // 15KB default buffer
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;

    pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
    if (!pAddresses) return links;

    DWORD dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
    }

    if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
        int netIndex = 0;   // physical links: net0, net1, ...
        int vnicIndex = 0;  // virtual NICs: vnic0, vnic1, ...
        int aggrIndex = 0;  // aggregations: aggr0, aggr1, ...
        int vlanIndex = 0;  // VLANs: vlan0, vlan1, ...

        while (pCurr) {
            std::wstring wsFriendly(pCurr->FriendlyName);
            const std::string sFriendly = WStringToUtf8(wsFriendly);
            const std::string adapterName = sFriendly;

            if (!IsSyntheticWindowsInterface(adapterName, adapterName, pCurr->IfType)) {
                DataLinkInfo info;
                std::ostringstream linkNameOss;

                info.adapterName = adapterName;

                if (pCurr->IfType == IF_TYPE_IEEE80211) {
                    info.media = "Wi-Fi";
                    info.linkClass = LinkClass::Phys;
                    linkNameOss << "net" << netIndex++;
                } else if (adapterName.find("vEthernet") != std::string::npos || adapterName.find("Hyper-V") != std::string::npos) {
                    info.media = "Ethernet";
                    info.linkClass = LinkClass::Vnic;
                    linkNameOss << "vnic" << vnicIndex++;
                } else if (adapterName.find("Multiplexor") != std::string::npos || adapterName.find("Team") != std::string::npos) {
                    info.media = "Ethernet";
                    info.linkClass = LinkClass::Aggr;
                    linkNameOss << "aggr" << aggrIndex++;
                } else {
                    info.media = "Ethernet";
                    info.linkClass = LinkClass::Phys;
                    linkNameOss << "net" << netIndex++;
                }

                info.linkName = linkNameOss.str();
                info.mtu = pCurr->Mtu;
                info.macAddress = FormatMacAddress(pCurr->PhysicalAddress, pCurr->PhysicalAddressLength);

                if (pCurr->OperStatus == IfOperStatusUp) {
                    info.state = "up";
                } else if (pCurr->OperStatus == IfOperStatusDown) {
                    info.state = "down";
                } else {
                    info.state = "unknown";
                }

                MIB_IF_ROW2 row2{};
                row2.InterfaceLuid = pCurr->Luid;
                if (GetIfEntry2(&row2) == NO_ERROR) {
                    info.speedBps = row2.ReceiveLinkSpeed;
                    info.duplex = (row2.MediaConnectState == MediaConnectStateConnected) ? "full" : "unknown";
                } else {
                    info.speedBps = pCurr->TransmitLinkSpeed;
                    info.duplex = "full";
                }

                info.deviceName = pCurr->AdapterName;
                links.push_back(info);
            }
            pCurr = pCurr->Next;
        }
    }

    if (pAddresses) free(pAddresses);

    // Resolve overLink for VNICs/Aggr/VLANs to the first physical link's name.
    // (Windows does not expose the exact parent adapter via the IP Helper API,
    // so we bind to the first 'up' physical link, falling back to net0.)
    std::string firstPhys = "";
    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Phys && l.state == "up") { firstPhys = l.linkName; break; }
    }
    if (firstPhys.empty()) {
        for (const auto& l : links) {
            if (l.linkClass == LinkClass::Phys) { firstPhys = l.linkName; break; }
        }
    }
    for (auto& l : links) {
        if ((l.linkClass == LinkClass::Vnic || l.linkClass == LinkClass::Aggr || l.linkClass == LinkClass::Vlan) &&
            (l.overLink.empty() || l.overLink == "--")) {
            l.overLink = firstPhys.empty() ? "--" : firstPhys;
        }
    }

    return links;
}

} // namespace dladm
