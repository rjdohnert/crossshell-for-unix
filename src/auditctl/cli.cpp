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

#include "cli.hpp"
#include <iostream>

void PrintInvalidUsage(const wchar_t* message) {
    std::wcerr << L"auditctl: error: " << message << L"\n\n";
    ShowHelp();
}

void ShowHelp() {
    std::wcout << LR"(auditctl(1)             CrossShell for UNIX Reference Manual            auditctl(1)

    NAME
        auditctl - Display and update Windows Advanced Audit Policy.

    SYNOPSIS
        auditctl [--json | --csv | --table] [--pipe COMMAND] --status
        auditctl [--json | --csv | --table] [--pipe COMMAND] --list-classes
        auditctl [--dry-run] [--json | --csv | --table] [--pipe COMMAND]
                 (--enable | --disable) CLASS
        auditctl --help

    DESCRIPTION
        Displays and updates selected Windows Advanced Audit Policy
        subcategories using Solaris-style class names. Policy data is read
        and changed through the Windows Audit Policy APIs. Output uses the
        Windows console and standard streams and can be formatted or piped
        to another command.

    OPTIONS
        -s, --status
            Show the current audit state for all supported classes.

        -e, --enable CLASS
            Enable Success and Failure auditing for CLASS.

        -d, --disable CLASS
            Disable auditing for CLASS.

        -l, --list-classes
            List supported class names and Solaris-style aliases.

        -n, --dry-run
            Print intended enable or disable changes without applying them.

        --json
            Format command output as JSON.

        --csv
            Format command output as CSV.

        --table
            Format command output as a table.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -h, --help
            Display this reference manual.

    AVAILABLE CLASSES
        proc, ex
            Process Creation and Execution events.

        file, fc
            File System object access events.

        logon, logoff, lo
            Logon and logoff events. The lo alias selects both classes.

        account, policy, ad
            Account management and audit policy changes. The ad alias
            selects both classes.

        all
            Select every supported class.

    NOTES
        Policy changes require Administrator rights and SeSecurityPrivilege.
        Status queries also require elevation. Enabling a class selects both
        Success and Failure auditing; disabling selects No Auditing.

    EXAMPLES
        auditctl --status
            Display the current state of every supported audit class.

        auditctl --enable proc
            Enable Success and Failure auditing for process creation.

        auditctl --dry-run --disable all
            Preview disabling every supported audit class.

        auditctl --json --status
            Display audit status as JSON.

        auditctl --csv --pipe "more" --status
            Send CSV-formatted audit status through another command.

    CrossShell for UNIX                                                     auditctl(1)
    )";
}
