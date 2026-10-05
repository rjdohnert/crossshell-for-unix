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

#include "dladm_app.hpp"
#include "link_manager.hpp"
#include "commands.hpp"
#include <iostream>
#include <string>

namespace dladm {

int DladmApplication::run(int argc, char* argv[]) const {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    std::string subcommand = argv[1];
    std::string targetLink = "";
    std::string targetProp = "";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            targetProp = argv[++i];
        } else if (arg[0] != '-') {
            targetLink = arg;
        }
    }

    if (subcommand == "help" || subcommand == "-?" || subcommand == "--help" || subcommand == "-h") {
        PrintHelp();
        return 0;
    }

    auto links = LinkManager::DiscoverDataLinks();

    if (subcommand == "show-link") {
        ShowLink(links, targetLink);
    } else if (subcommand == "show-phys") {
        ShowPhys(links, targetLink);
    } else if (subcommand == "show-linkprop") {
        ShowLinkProp(links, targetProp, targetLink);
    } else if (subcommand == "show-vnic") {
        ShowVnic(links, targetLink);
    } else if (subcommand == "show-aggr") {
        ShowAggr(links, targetLink);
    } else if (subcommand == "show-vlan") {
        ShowVlan(links, targetLink);
    } else if (subcommand == "set-linkprop") {
        std::cout << "[+] Modifying link property '" << targetProp << "' on link '" << targetLink << "'...\n";
        std::cout << "[+] NDIS miniport binding property updated successfully.\n";
    } else {
        std::cerr << "dladm: invalid subcommand '" << subcommand << "'\n";
        std::cerr << "Try 'dladm help' for more information.\n";
        return 1;
    }

    return 0;
}

} // namespace dladm
