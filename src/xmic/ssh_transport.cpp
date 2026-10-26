#include "xmic_common.hpp"

std::wstring BuildSshTarget(const WmiTargetConfig& target) {
    if (target.user.empty()) {
        return target.node;
    }
    return target.user + L"@" + target.node;
}
std::wstring BuildSshControlPathTemplate() {
    wchar_t tempDir[MAX_PATH] = {};
    DWORD tempLen = GetTempPathW(static_cast<DWORD>(std::size(tempDir)), tempDir);
    if (tempLen == 0 || tempLen >= std::size(tempDir)) {
        return L"";
    }

    std::wstring templatePath = tempDir;
    if (!templatePath.empty() && templatePath.back() != L'\\' && templatePath.back() != L'/') {
        templatePath += L"\\";
    }
    templatePath += L"xmic-ssh-%C";
    return templatePath;
}
std::wstring QuoteShellArg(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = value.find_first_of(L" \t\"") != std::wstring::npos;
    if (!needsQuotes) {
        return value;
    }

    std::wstring quoted = L"\"";
    size_t backslashCount = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashCount;
            continue;
        }

        if (ch == L'\"') {
            quoted.append(backslashCount * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            backslashCount = 0;
            continue;
        }

        if (backslashCount > 0) {
            quoted.append(backslashCount, L'\\');
            backslashCount = 0;
        }

        quoted.push_back(ch);
    }

    if (backslashCount > 0) {
        quoted.append(backslashCount * 2, L'\\');
    }

    quoted += L"\"";
    return quoted;
}
std::wstring TrimWhitespace(const std::wstring& value) {
    size_t start = 0;
    while (start < value.size() && iswspace(value[start])) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && iswspace(value[end - 1])) {
        --end;
    }

    return value.substr(start, end - start);
}
std::vector<std::wstring> SplitCommaTrimmed(const std::wstring& input) {
    std::vector<std::wstring> parts;
    parts.reserve(4);

    size_t tokenStart = 0;
    while (tokenStart <= input.size()) {
        size_t tokenEnd = input.find(L',', tokenStart);
        if (tokenEnd == std::wstring::npos) {
            tokenEnd = input.size();
        }

        size_t start = tokenStart;
        while (start < tokenEnd && iswspace(input[start])) {
            ++start;
        }

        size_t end = tokenEnd;
        while (end > start && iswspace(input[end - 1])) {
            --end;
        }

        if (end > start) {
            parts.emplace_back(input.substr(start, end - start));
        }

        if (tokenEnd == input.size()) {
            break;
        }

        tokenStart = tokenEnd + 1;
    }

    return parts;
}
bool StartsWithCaseInsensitive(const std::wstring& value, const std::wstring& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (size_t i = 0; i < prefix.size(); ++i) {
        if (towlower(value[i]) != towlower(prefix[i])) {
            return false;
        }
    }

    return true;
}
std::wstring BuildWqlWithOptionalWhere(const std::wstring& baseQuery, const std::wstring& whereClause) {
    if (whereClause.empty()) {
        return baseQuery;
    }

    std::wstring lower = baseQuery;
    std::transform(lower.begin(), lower.end(), lower.begin(), towlower);

    if (lower.find(L" where ") != std::wstring::npos) {
        return baseQuery + L" AND (" + whereClause + L")";
    }

    return baseQuery + L" WHERE " + whereClause;
}
bool TryBuildInstanceSelectQueryFromObjectPath(const std::wstring& objectPath, std::wstring& queryOut) {
    queryOut.clear();

    size_t dotPos = objectPath.find(L'.');
    size_t eqPos = objectPath.find(L'=');
    if (dotPos == std::wstring::npos || eqPos == std::wstring::npos || dotPos == 0 || dotPos + 1 >= objectPath.size()) {
        return false;
    }

    std::wstring className = objectPath.substr(0, dotPos);
    std::wstring predicate = objectPath.substr(dotPos + 1);
    if (className.empty() || predicate.empty()) {
        return false;
    }

    // Convert key list separators to boolean conjunction for a WQL predicate.
    std::wstring normalizedPredicate;
    normalizedPredicate.reserve(predicate.size() + 16);
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;
    for (wchar_t ch : predicate) {
        if (escaped) {
            normalizedPredicate += ch;
            escaped = false;
            continue;
        }

        if (ch == L'\\') {
            escaped = true;
            normalizedPredicate += ch;
            continue;
        }

        if (ch == L'\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            normalizedPredicate += ch;
            continue;
        }
        if (ch == L'\"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            normalizedPredicate += ch;
            continue;
        }

        if (ch == L',' && !inSingleQuote && !inDoubleQuote) {
            normalizedPredicate += L" AND ";
        } else {
            normalizedPredicate += ch;
        }
    }

    queryOut = L"SELECT * FROM " + className + L" WHERE " + normalizedPredicate;
    return true;
}
bool TryGetEnvironmentVariableValue(const wchar_t* name, std::wstring& value) {
    value.clear();
    DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return false;
    }

    std::vector<wchar_t> buffer(required);
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), required);
    if (written == 0 || written >= required) {
        return false;
    }

    value.assign(buffer.data(), written);
    return true;
}
bool CreateUniqueTempFile(const wchar_t* prefix, const wchar_t* extension, ScopedTempFile& tempFile) {
    wchar_t tempDir[MAX_PATH] = {};
    DWORD tempLen = GetTempPathW(static_cast<DWORD>(std::size(tempDir)), tempDir);
    if (tempLen == 0 || tempLen >= std::size(tempDir)) {
        return false;
    }

    ULONGLONG tick = GetTickCount64();
    DWORD pid = GetCurrentProcessId();
    for (int attempt = 0; attempt < 64; ++attempt) {
        std::wstring candidate = tempDir;
        candidate += prefix;
        candidate += L"-";
        candidate += std::to_wstring(pid);
        candidate += L"-";
        candidate += std::to_wstring(tick);
        candidate += L"-";
        candidate += std::to_wstring(attempt);
        candidate += extension;
        HANDLE file = CreateFileW(
            candidate.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL
        );
        if (file != INVALID_HANDLE_VALUE) {
            tempFile.Adopt(candidate, file);
            return true;
        }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }

    return false;
}
bool WriteUtf8TextFile(ScopedTempFile& tempFile, const std::wstring& content) {
    int byteCount = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), static_cast<int>(content.size()), NULL, 0, NULL, NULL);
    if (byteCount <= 0) {
        return false;
    }

    std::vector<char> bytes;
    bytes.reserve(static_cast<size_t>(byteCount) + 3);
    bytes.push_back(static_cast<char>(0xEF));
    bytes.push_back(static_cast<char>(0xBB));
    bytes.push_back(static_cast<char>(0xBF));
    size_t start = bytes.size();
    bytes.resize(start + static_cast<size_t>(byteCount));
    if (WideCharToMultiByte(CP_UTF8, 0, content.c_str(), static_cast<int>(content.size()), bytes.data() + start, byteCount, NULL, NULL) <= 0) {
        return false;
    }

    if (bytes.size() > MAXDWORD) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(tempFile.Handle(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL);
    return ok && written == bytes.size() && FlushFileBuffers(tempFile.Handle()) && tempFile.SealForReading();
}
bool CreateSshAskPassHelper(ScopedTempFile& helperFile) {
    std::wstring powershellPath;
    if (!ResolveSystemExecutablePath(L"WindowsPowerShell\\v1.0\\powershell.exe", powershellPath)) return false;
    std::wstring helper = L"@echo off\r\n\"" + powershellPath +
        L"\" -NoProfile -ExecutionPolicy Bypass -Command \"[Console]::Write($env:XMIC_SSH_PASSPHRASE)\"\r\n";
    int byteCount = WideCharToMultiByte(CP_ACP, 0, helper.c_str(), static_cast<int>(helper.size()), NULL, 0, NULL, NULL);
    if (byteCount <= 0) return false;
    std::vector<char> bytes(static_cast<size_t>(byteCount));
    if (WideCharToMultiByte(CP_ACP, 0, helper.c_str(), static_cast<int>(helper.size()), bytes.data(), byteCount, NULL, NULL) <= 0) return false;
    DWORD written = 0;
    return bytes.size() <= MAXDWORD && WriteFile(helperFile.Handle(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) &&
        written == bytes.size() && FlushFileBuffers(helperFile.Handle()) && helperFile.SealForReading();
}
bool RunSshCommandCaptureWithOptionalPassphrase(const WmiTargetConfig& target, const std::wstring& sshCommand, std::wstring& output, int& exitCode) {
    if (sshCommand.empty()) {
        exitCode = -1;
        return false;
    }
    if (target.sshKeyPassphrase.empty()) {
        return RunCommandCapture(sshCommand, output, exitCode);
    }

    ScopedTempFile askPassFile;
    if (!CreateUniqueTempFile(L"xmic-ssh-askpass", L".cmd", askPassFile)) {
        exitCode = -1;
        return false;
    }

    if (!CreateSshAskPassHelper(askPassFile)) {
        exitCode = -1;
        return false;
    }

    ScopedEnvironmentVariable passphraseEnv(L"XMIC_SSH_PASSPHRASE", target.sshKeyPassphrase);
    ScopedEnvironmentVariable askPassEnv(L"SSH_ASKPASS", askPassFile.Path());
    ScopedEnvironmentVariable askPassRequireEnv(L"SSH_ASKPASS_REQUIRE", L"force");
    ScopedEnvironmentVariable displayEnv(L"DISPLAY", L"1");
    return RunCommandCapture(sshCommand, output, exitCode);
}
std::wstring NormalizePathForComparison(const std::wstring& path) {
    if (path.empty()) {
        return path;
    }

    DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        std::wstring lowered = path;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
        return lowered;
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetFullPathNameW(path.c_str(), needed, buffer.data(), nullptr);
    if (written == 0 || written >= needed) {
        std::wstring lowered = path;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
        return lowered;
    }

    std::wstring normalized(buffer.data(), written);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), towlower);
    return normalized;
}
bool ContainsPathInSemicolonList(const std::wstring& list, const std::wstring& candidate) {
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(L';', start);
        if (end == std::wstring::npos) {
            end = list.size();
        }

        std::wstring item = TrimWhitespace(list.substr(start, end - start));
        if (!item.empty() && item == candidate) {
            return true;
        }

        start = end + 1;
    }

    return false;
}
std::wstring AppendPathToSemicolonList(const std::wstring& list, const std::wstring& path) {
    if (path.empty()) {
        return list;
    }

    if (list.empty()) {
        return path;
    }

    if (ContainsPathInSemicolonList(list, path)) {
        return list;
    }

    return list + L";" + path;
}
bool TryParseScriptDepth(const std::wstring& value, int& depth) {
    if (value.empty()) {
        return false;
    }

    try {
        size_t processed = 0;
        int parsed = std::stoi(value, &processed);
        if (processed != value.size() || parsed < 0) {
            return false;
        }
        depth = parsed;
        return true;
    } catch (...) {
        return false;
    }
}
std::wstring SanitizeScriptCommandForLog(const std::wstring& commandLine) {
    std::wstring lowered = commandLine;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);

    if (lowered.find(L"--password") != std::wstring::npos ||
        lowered.find(L"/password:") != std::wstring::npos) {
        return L"[command redacted: contains password argument]";
    }

    return commandLine;
}
bool ExecuteScriptFile(const std::wstring& scriptPath, const std::wstring& executablePath, bool stopOnError, bool echoCommands, int currentScriptDepth, const std::wstring& inheritedScriptStack, int& exitCodeOut) {
    exitCodeOut = EXIT_OK;

    if (currentScriptDepth >= kMaxScriptDepth) {
        std::wcerr << L"[!] Script recursion depth limit exceeded (max " << kMaxScriptDepth << L")." << std::endl;
        exitCodeOut = EXIT_BAD_ARGS;
        return false;
    }

    std::wstring normalizedScriptPath = NormalizePathForComparison(scriptPath);
    std::wstring childScriptStack = AppendPathToSemicolonList(inheritedScriptStack, normalizedScriptPath);
    int childScriptDepth = currentScriptDepth + 1;

    std::wifstream scriptFile;
    scriptFile.open(scriptPath.c_str(), std::ios::in);
    if (!scriptFile.is_open()) {
        std::wcerr << L"[!] Failed to open script file: " << scriptPath << std::endl;
        exitCodeOut = EXIT_BAD_ARGS;
        return false;
    }

    scriptFile.imbue(std::locale(""));

    bool hasError = false;
    std::wstring resolvedExecutablePath;
    if (!ResolveExecutablePath(executablePath, resolvedExecutablePath)) {
        exitCodeOut = EXIT_WMI_FAILURE;
        return false;
    }
    std::wstring executableToken = resolvedExecutablePath;
    wchar_t shortPathBuffer[32768] = {};
    DWORD shortPathLen = GetShortPathNameW(resolvedExecutablePath.c_str(), shortPathBuffer, static_cast<DWORD>(std::size(shortPathBuffer)));
    if (shortPathLen > 0 && shortPathLen < std::size(shortPathBuffer)) {
        executableToken.assign(shortPathBuffer, shortPathLen);
    }

    std::wstring line;
    int lineNumber = 0;

    while (std::getline(scriptFile, line)) {
        ++lineNumber;
        std::wstring commandLine = TrimWhitespace(line);
        if (commandLine.empty() || commandLine[0] == L'#' || commandLine[0] == L';') {
            continue;
        }

        bool looksLikeFullXmCommand =
            StartsWithCaseInsensitive(commandLine, L"xmic") ||
            StartsWithCaseInsensitive(commandLine, L".\\xmic") ||
            StartsWithCaseInsensitive(commandLine, L"\"xmic") ||
            StartsWithCaseInsensitive(commandLine, L"\".\\xmic");

        size_t commandTokenEnd = 0;
        if (!commandLine.empty() && commandLine.front() == L'"') {
            commandTokenEnd = commandLine.find(L'"', 1);
            if (commandTokenEnd == std::wstring::npos) commandTokenEnd = commandLine.size();
            else ++commandTokenEnd;
        } else {
            commandTokenEnd = commandLine.find_first_of(L" \t");
            if (commandTokenEnd == std::wstring::npos) commandTokenEnd = commandLine.size();
        }
        std::wstring coreCommand;
        if (looksLikeFullXmCommand) {
            size_t argumentsStart = commandTokenEnd;
            while (argumentsStart < commandLine.size() && iswspace(commandLine[argumentsStart])) ++argumentsStart;
            coreCommand = QuoteCommandLineArgument(executableToken);
            if (argumentsStart < commandLine.size()) coreCommand += L" " + commandLine.substr(argumentsStart);
        } else {
            coreCommand = QuoteCommandLineArgument(executableToken) + L" " + commandLine;
        }
        std::wstring fullCommand = coreCommand;

        std::wstring output;
        int commandExitCode = -1;
        ScopedEnvironmentVariable depthGuard(kScriptDepthEnvVar, std::to_wstring(childScriptDepth));
        ScopedEnvironmentVariable stackGuard(kScriptStackEnvVar, childScriptStack);
        bool ok = RunCommandCapture(fullCommand, output, commandExitCode);

        if (echoCommands) {
            std::wstring visibleCommand = SanitizeScriptCommandForLog(commandLine);
            std::wcout << L"\n[script line " << lineNumber << L"] " << visibleCommand << L"\n";
        }
        if (!output.empty()) {
            std::wcout << output;
            if (output.back() != L'\n') {
                std::wcout << L"\n";
            }
        }

        if (!ok || commandExitCode != EXIT_OK) {
            hasError = true;
            exitCodeOut = (commandExitCode >= 0) ? commandExitCode : EXIT_WMI_FAILURE;
            std::wcerr << L"[!] Script command failed at line " << lineNumber
                       << L" with exit code " << exitCodeOut << L"." << std::endl;
            if (stopOnError) {
                break;
            }
        }
    }

    if (!hasError) {
        exitCodeOut = EXIT_OK;
        return true;
    }

    return false;
}
std::wstring BuildSshBaseCommand(const WmiTargetConfig& target) {
    std::wstring sshPath;
    if (!ResolveSystemExecutablePath(L"OpenSSH\\ssh.exe", sshPath) &&
        !ResolveExecutableOnPath(L"ssh.exe", sshPath)) return L"";
    std::wstring sshCommand = QuoteCommandLineArgument(sshPath);

    std::wstring controlPath = BuildSshControlPathTemplate();
    if (!controlPath.empty()) {
        sshCommand += L" -o " + QuoteShellArg(L"ControlMaster=auto");
        sshCommand += L" -o " + QuoteShellArg(L"ControlPersist=60");
        sshCommand += L" -o " + QuoteShellArg(L"ControlPath=" + controlPath);
    }

    if (target.sshPort > 0 && target.sshPort != 22) {
        sshCommand += L" -p " + std::to_wstring(target.sshPort);
    }

    if (!target.sshKeyFile.empty()) {
        sshCommand += L" -i " + QuoteShellArg(target.sshKeyFile);
    }

    if (!target.sshStrictHostKeyChecking.empty()) {
        sshCommand += L" -o " + QuoteShellArg(L"StrictHostKeyChecking=" + target.sshStrictHostKeyChecking);
    }

    if (!target.sshKnownHostsFile.empty()) {
        sshCommand += L" -o " + QuoteShellArg(L"UserKnownHostsFile=" + target.sshKnownHostsFile);
    }

    if (!target.sshProxyJump.empty()) {
        sshCommand += L" -J " + QuoteShellArg(target.sshProxyJump);
    }

    sshCommand += L" " + QuoteShellArg(BuildSshTarget(target));
    return sshCommand;
}
bool RunSshPowerShellScriptCapture(const WmiTargetConfig& target, const std::wstring& script, std::wstring& output, int& exitCode) {
    std::wstring sshBaseCommand = BuildSshBaseCommand(target);
    if (sshBaseCommand.empty()) {
        exitCode = -1;
        return false;
    }
    std::wstring encoded = EncodePowerShellCommandBase64(script);
    std::wstring remoteCommand = L"powershell -NoProfile -ExecutionPolicy Bypass -EncodedCommand " + encoded;
    std::wstring sshCommand = sshBaseCommand + L" " + QuoteShellArg(remoteCommand);
    return RunSshCommandCaptureWithOptionalPassphrase(target, sshCommand, output, exitCode);
}
bool RunSshConnectivityPreflight(const WmiTargetConfig& target, std::wstring& details) {
    details.clear();
    std::wstring output;
    int exitCode = -1;
    std::wstring sshBaseCommand = BuildSshBaseCommand(target);
    if (sshBaseCommand.empty()) {
        details = L"OpenSSH executable was not found on PATH.";
        return false;
    }
    std::wstring command = sshBaseCommand + L" " + QuoteShellArg(L"echo XMIC_SSH_OK");

    if (!RunSshCommandCaptureWithOptionalPassphrase(target, command, output, exitCode) || exitCode != 0) {
        if (!output.empty()) {
            details = output;
        } else {
            details = L"ssh command failed before completing the remote preflight check.";
        }
        return false;
    }

    if (output.find(L"XMIC_SSH_OK") == std::wstring::npos) {
        details = output.empty()
            ? L"preflight did not return expected marker from remote shell."
            : L"unexpected preflight response: " + output;
        return false;
    }

    details = L"Connected";
    return true;
}
std::wstring NormalizeSshErrorText(const std::wstring& rawText) {
    std::wstring lowered = rawText;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);

    if (lowered.find(L"permission denied") != std::wstring::npos) {
        return L"SSH authentication failed: permission denied.";
    }
    if (lowered.find(L"connection timed out") != std::wstring::npos || lowered.find(L"operation timed out") != std::wstring::npos) {
        return L"SSH transport timed out.";
    }
    if (lowered.find(L"could not resolve hostname") != std::wstring::npos || lowered.find(L"name or service not known") != std::wstring::npos) {
        return L"SSH host resolution failed.";
    }
    if (lowered.find(L"connection refused") != std::wstring::npos) {
        return L"SSH connection was refused by the remote host.";
    }
    if (lowered.find(L"powershell") != std::wstring::npos && lowered.find(L"not recognized") != std::wstring::npos) {
        return L"Remote PowerShell is not available on the SSH target.";
    }
    if (lowered.find(L"get-ciminstance") != std::wstring::npos || lowered.find(L"cim") != std::wstring::npos) {
        return L"Remote CIM/WMI execution failed over SSH.";
    }

    return rawText;
}
