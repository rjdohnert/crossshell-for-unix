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

#include "executor.hpp"
#include "path_helper.hpp"
#include "models.hpp"
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int ChrootExecutor::ExecuteSandbox(const ChrootOptions& options) {
    if (options.positional.empty()) {
        options.PrintHelp();
        return 1;
    }

    std::wstring newRoot = PathHelper::GetAbsolutePath(options.positional[0]);

    if (!PathHelper::DirectoryExists(newRoot)) {
        std::wcerr << L"chroot: cannot change root directory to '" << newRoot 
                   << L"': No such file or directory\n";
        return 1;
    }

    std::wstring command = L"";
    std::vector<std::wstring> commandArgs;

    if (options.positional.size() >= 2) {
        command = options.positional[1];
        for (size_t i = 2; i < options.positional.size(); ++i) {
            commandArgs.push_back(options.positional[i]);
        }
    } else {
        command = L"cmd.exe";
    }

    std::wstring commandLine = PathHelper::BuildCommandLine(command, commandArgs);

    if (!EnvironmentConfigurator::ConfigureSandbox(newRoot)) {
        std::wcerr << L"chroot: failed to set current directory to " << newRoot << L"\n";
        return 1;
    }

#ifdef _WIN32
    ScopedJobHandle hJob(CreateJobObjectW(NULL, NULL));
    if (hJob.IsValid()) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob.Get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    STARTUPINFOW si = { sizeof(si) };
    ScopedProcessInfo pi;

    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        newRoot.c_str(),
        &si,
        &pi.pi
    );

    if (!success) {
        DWORD err = GetLastError();
        std::wcerr << L"chroot: failed to execute command '" << command 
                   << L"' (Error Code: " << err << L")\n";
        return 1;
    }

    if (hJob.IsValid()) {
        AssignProcessToJobObject(hJob.Get(), pi.pi.hProcess);
    }

    WaitForSingleObject(pi.pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.pi.hProcess, &exitCode);

    if (options.outputFormat != OutputFormat::Default || !options.pipeCommand.empty()) {
        std::wstring text = (options.outputFormat == OutputFormat::Json) ? L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(exitCode) + L"}\n" :
                            (options.outputFormat == OutputFormat::Csv) ? L"status,exit_code\ncompleted," + std::to_wstring(exitCode) + L"\n" :
                            L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(exitCode) + L"\n";
        if (!options.pipeCommand.empty()) {
            ScopedWpOpen pipe(_wpopen(options.pipeCommand.c_str(), L"w"));
            if (pipe.IsValid()) {
                std::string narrow = PathHelper::Utf8(text);
                fwrite(narrow.data(), 1, narrow.size(), pipe.Get());
            }
        } else {
            std::wcout << text;
        }
    }

    return static_cast<int>(exitCode);
#else
    return 0;
#endif
}
