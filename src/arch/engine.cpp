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
#include "reporter.hpp"
#include <utility>

std::string SystemArchResolver::getArchString(WORD archVal) {
    switch (archVal) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "amd64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "i386";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "arm64";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "arm";
        case PROCESSOR_ARCHITECTURE_IA64:
            return "ia64";
        default:
            return "unknown";
    }
}

ArchEngine::ArchEngine(ArchOptions opts) : options(std::move(opts)) {}

int ArchEngine::execute() {
    SYSTEM_INFO sysInfo;
    SYSTEM_INFO nativeSysInfo;
    GetSystemInfo(&sysInfo);
    GetNativeSystemInfo(&nativeSysInfo);

    std::string archStr;
    if (options.asMachine) {
        WORD targetArch = nativeSysInfo.wProcessorArchitecture;
        if (options.optApp) {
            targetArch = sysInfo.wProcessorArchitecture;
        }
        archStr = SystemArchResolver::getArchString(targetArch);
    } else {
        WORD targetArch = sysInfo.wProcessorArchitecture;
        if (options.optKernel) {
            targetArch = nativeSysInfo.wProcessorArchitecture;
        }
        archStr = SystemArchResolver::getArchString(targetArch);
    }

    return ArchReporter::dispatch(archStr, options.outputFormat, options.pipeCommand);
}
