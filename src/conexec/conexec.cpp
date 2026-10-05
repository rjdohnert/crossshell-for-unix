/*
BSD 3 Clause License
--------------------

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer
in the documentation and/or other materials provided with the distribution. Neither the name of [project] nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

Disclaimer:
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAG
*/

#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <aclapi.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "userenv.lib")

namespace fs = std::filesystem;

#define RUNCON_VERSION L"3.0.0"
#define RUNCON_AUTHOR  L"Roberto J Dohnert"

// --- Global State for Signal Cleanup ---
namespace GlobalState {
    std::atomic<bool> isTerminating{ false };
    std::atomic<bool> signalCleanupStarted{ false };
    std::mutex cleanupMutex;
    fs::path activeWorkspace;
    HANDLE activeJob = nullptr;
    std::wstring appContainerProfileName;
}

#include "conexec_module_01_isolationlevel.inc"
#include "conexec_module_02_runconconfig.inc"
#include "conexec_module_03_filesnapshot.inc"
#include "conexec_module_04_handlecloser.inc"
#include "conexec_module_05_securityengine.inc"
#include "conexec_module_06_executiondispatcher.inc"
