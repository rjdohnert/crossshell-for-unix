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

#include "commands.hpp"
#include <iostream>
#include <iomanip>

namespace dladm {

void PrintHelp() {
    std::cout << R"(dladm(1)                   CrossShell for UNIX Reference Manual                   dladm(1)

    NAME
        dladm - administer Windows data links and virtual network links

    SYNOPSIS
        dladm SUBCOMMAND [OPTIONS] [LINK]

    DESCRIPTION
        Emulates Solaris dladm using Windows NDIS, Hyper-V virtual links, VLAN,
        aggregation, VNIC, MTU, speed, and duplex information.

    OPTIONS
        show-link
            Display generic data-link information.
        show-phys
            Display physical adapter state and speed.
        show-linkprop
            Display tunable link properties.
        set-linkprop
            Modify a link property.
        show-vlan
            Display VLAN configuration.
        show-aggr
            Display aggregation or NIC teaming state.
        show-vnic
            Display virtual network interfaces.
        -p PROPERTY
            Select a property for show/set-linkprop.
        --json, --csv, --table
            Select structured output format.
        --pipe COMMAND
            Send formatted output through COMMAND.
        -h, --help, help, -?
            Display this reference manual and exit.

    EXAMPLES
        dladm show-link
            Display summary of all network data links.

        dladm show-phys
            Display physical network interface state.

        dladm show-linkprop -p mtu net0
            Display MTU property for link 'net0'.

        dladm show-vnic
            Display Hyper-V virtual NIC bindings.

        dladm show-aggr --json
            Display link aggregations formatted as JSON.

    CrossShell for UNIX                                                    dladm(1)
)";
}

void ShowLink(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(10) << "CLASS"
              << std::setw(8)  << "MTU"
              << std::setw(10) << "STATE"
              << std::setw(12) << "BRIDGE"
              << "OVER\n";

    for (const auto& l : links) {
        if (targetLink.empty() || l.linkName == targetLink) {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(10) << TruncateString(LinkClassToString(l.linkClass), 10)
                      << std::setw(8)  << l.mtu
                      << std::setw(10) << TruncateString(l.state, 10)
                      << std::setw(12) << "--"
                      << TruncateString(l.overLink, 24) << "\n";
        }
    }
}

void ShowPhys(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(14) << "MEDIA"
              << std::setw(10) << "STATE"
              << std::setw(10) << "SPEED"
              << std::setw(10) << "DUPLEX"
              << "DEVICE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Phys) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(14) << TruncateString(l.media, 14)
                          << std::setw(10) << TruncateString(l.state, 10)
                          << std::setw(10) << TruncateString(FormatSpeed(l.speedBps), 10)
                          << std::setw(10) << TruncateString(l.duplex, 10)
                          << TruncateString(l.adapterName, 40) << "\n";
            }
        }
    }
}

void ShowLinkProp(const std::vector<DataLinkInfo>& links, const std::string& targetProp, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(16) << "PROPERTY"
              << std::setw(8)  << "PERM"
              << std::setw(18) << "VALUE"
              << std::setw(14) << "DEFAULT"
              << "POSSIBLE\n";

    for (const auto& l : links) {
        if (!targetLink.empty() && l.linkName != targetLink) continue;

        if (targetProp.empty() || targetProp == "mtu") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "mtu"
                      << std::setw(8)  << "rw"
                      << std::setw(18) << TruncateString(std::to_string(l.mtu), 18)
                      << std::setw(14) << "1500"
                      << "1500-9000\n";
        }
        if (targetProp.empty() || targetProp == "speed") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "speed"
                      << std::setw(8)  << "r-"
                      << std::setw(18) << TruncateString(FormatSpeed(l.speedBps), 18)
                      << std::setw(14) << TruncateString(FormatSpeed(l.speedBps), 14)
                      << "10,100,1000,10000\n";
        }
        if (targetProp.empty() || targetProp == "duplex") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "duplex"
                      << std::setw(8)  << "r-"
                      << std::setw(18) << TruncateString(l.duplex, 18)
                      << std::setw(14) << "full"
                      << "half,full\n";
        }
        if (targetProp.empty() || targetProp == "mac-address") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "mac-address"
                      << std::setw(8)  << "rw"
                      << std::setw(18) << TruncateString(l.macAddress, 18)
                      << std::setw(14) << TruncateString(l.macAddress, 14)
                      << "--\n";
        }
    }
}

void ShowVnic(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(12) << "OVER"
              << std::setw(10) << "SPEED"
              << std::setw(20) << "MACADDRESS"
              << "MACADDRTYPE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Vnic) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(12) << TruncateString(l.overLink, 12)
                          << std::setw(10) << TruncateString(FormatSpeed(l.speedBps), 10)
                          << std::setw(20) << TruncateString(l.macAddress, 20)
                          << "fixed\n";
            }
        }
    }
}

void ShowAggr(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(12) << "POLICY"
              << std::setw(14) << "ADDRPOLICY"
              << std::setw(12) << "LACPACTIVE"
              << "LACPMODE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Aggr) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(12) << "L4"
                          << std::setw(14) << "auto"
                          << std::setw(12) << "off"
                          << "off\n";
            }
        }
    }
}

void ShowVlan(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(8)  << "VID"
              << std::setw(12) << "OVER"
              << "FLAGS\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Vlan) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(8)  << l.vlanId
                          << std::setw(12) << TruncateString(l.overLink, 12)
                          << "-----\n";
            }
        }
    }
}

} // namespace dladm
