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

#include "chown_app.hpp"
#include "options.hpp"
#include "ownership.hpp"
#include "reporter.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int ChownApplication::Run(int argc, wchar_t* argv[]) const {
    ChownOptions options;
    if (!options.Parse(argc, argv)) {
        options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chown");
        return options.showHelp ? 0 : 1;
    }

    if (options.showHelp) {
        options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chown");
        return 0;
    }
    if (options.showVersion) {
        options.PrintVersion();
        return 0;
    }

    PrivilegeManager::EnableRequiredPrivileges();

    std::wstring ownerName, groupName;
    OwnershipSpecParser::Parse(options.specStr, ownerName, groupName);

#ifdef _WIN32
    ScopedSid pOwnerSid;
    ScopedSid pGroupSid;

    if (!ownerName.empty()) {
        pOwnerSid = SecurityAccountResolver::GetSidFromName(ownerName);
        if (!pOwnerSid.IsValid()) {
            if (!options.quiet) {
                std::wcerr << L"Error: Could not resolve User account: " << ownerName << L"\n";
            }
            return 1;
        }
    }

    if (!groupName.empty()) {
        pGroupSid = SecurityAccountResolver::GetSidFromName(groupName);
        if (!pGroupSid.IsValid()) {
            if (!options.quiet) {
                std::wcerr << L"Error: Could not resolve Group account: " << groupName << L"\n";
            }
            return 1;
        }
    }

    PSID ownerSidPtr = pOwnerSid.Get();
    PSID groupSidPtr = pGroupSid.Get();
#else
    void* ownerSidPtr = nullptr;
    void* groupSidPtr = nullptr;
#endif

    bool success = true;

    for (const auto& targetPath : options.targets) {
        fs::path target(targetPath);
        if (!fs::exists(target)) {
            std::wcerr << L"Error: Path does not exist: " << targetPath << L"\n";
            success = false;
            continue;
        }

        bool changed = false;
        if (options.recursive && fs::is_directory(target)) {
            std::error_code ec;
            auto iter = fs::recursive_directory_iterator(
                target,
                fs::directory_options::skip_permission_denied,
                ec
            );

            for (auto& entry : iter) {
                if (OwnershipManager::ApplyOwnership(entry.path(), ownerSidPtr, groupSidPtr, std::wcerr)) {
                    changed = true;
                } else {
                    success = false;
                }
            }
            if (OwnershipManager::ApplyOwnership(target, ownerSidPtr, groupSidPtr, std::wcerr)) {
                changed = true;
            } else {
                success = false;
            }
        } else {
            if (OwnershipManager::ApplyOwnership(target, ownerSidPtr, groupSidPtr, std::wcerr)) {
                changed = true;
            } else {
                success = false;
            }
        }

        if ((options.verbose || options.changes) && changed && !options.quiet) {
            std::wcout << L"changed ownership of '" << targetPath << L"' to '" << options.specStr << L"'\n";
        }
    }

    ChownReporter::OutputResults(success, options.format, options.pipeCommand);
    return success ? 0 : 1;
}
