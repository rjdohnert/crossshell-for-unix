#include "controllers.hpp"

// --- MountsToolController ---
void MountsToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --mounts [--json]\n"
            L"       nfsctl.exe --mount <remote-path> <drive:> [--confirm|--dry-run]\n"
            L"       nfsctl.exe --umount <drive:> [--confirm|--dry-run]\n");
}

DWORD MountsToolController::Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
    ULONGLONG startTick = GetTickCount64();

    std::wstring mount_path = L"", mount_drive = L"", umount_drive = L"";

    auto IsSwitchToken = [](const std::wstring& token) {
        return !token.empty() && token[0] == L'-';
    };

    const std::wstring command = args.empty() ? L"" : args[0];
    if (command == L"--mount") {
        if (args.size() != 3 || IsSwitchToken(args[1]) || IsSwitchToken(args[2])) {
            fwprintf(stderr, L"Error: Usage: nfsctl.exe --mount <remote-path> <drive:>\n");
            return ERROR_INVALID_PARAMETER;
        }
        mount_path = args[1];
        mount_drive = args[2];
    } else if (command == L"--umount") {
        if (args.size() != 2 || IsSwitchToken(args[1])) {
            fwprintf(stderr, L"Error: Usage: nfsctl.exe --umount <drive:>\n");
            return ERROR_INVALID_PARAMETER;
        }
        umount_drive = args[1];
    } else if (command == L"--mounts" || command == L"-m") {
        if (args.size() != 1) {
            fwprintf(stderr, L"Error: --mounts does not accept additional arguments.\n");
            return ERROR_INVALID_PARAMETER;
        }
    }

    if (!mount_path.empty() && !mount_drive.empty()) {
        bool shouldExecute = false;
        std::wstring dryRunMessage = L"[dry-run] Would mount " + mount_path + L" to " + mount_drive + L"\n";
        DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"mount operation", dryRunMessage, shouldExecute, false);
        if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

        NfsOutputFormatter::LogInfo(options, L"[*] Mounting %s to %s...\n", mount_path.c_str(), mount_drive.c_str());
        return NfsMountManager::WinMountNfsShare(mount_path.c_str(), mount_drive.c_str());
    }

    if (!umount_drive.empty()) {
        bool shouldExecute = false;
        std::wstring dryRunMessage = L"[dry-run] Would unmount " + umount_drive + L"\n";
        DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"unmount operation", dryRunMessage, shouldExecute, false);
        if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

        NfsOutputFormatter::LogInfo(options, L"[*] Unmounting %s...\n", umount_drive.c_str());
        return NfsMountManager::WinUnmountNfsShare(umount_drive.c_str());
    }

    if (options.outputMode == OutputMode::Text) {
        NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Connected Windows Network Drives ]\n\n");
    }

    std::vector<WinNfsMountInfo> mounts;
    DWORD dwErr = NfsMountManager::EnumWindowsNetworkMounts(mounts, options, startTick);
    if (dwErr != ERROR_SUCCESS) {
        if (options.outputMode == OutputMode::Json) {
            NfsOutputFormatter::PrintJsonEnvelope(L"mounts", dwErr, L"{\"mounts\":[],\"count\":0}");
        } else {
            NfsOutputFormatter::PrintWin32Error(L"failed to enumerate network mounts", dwErr);
        }
        return dwErr;
    }

    if (options.outputMode == OutputMode::Json) {
        std::wstringstream payload;
        payload << L"{\"mounts\":[";
        for (size_t i = 0; i < mounts.size(); ++i) {
            if (i > 0) payload << L",";
            payload << L"{\"localDrive\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].localDrive)
                    << L"\",\"remotePath\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].remotePath)
                    << L"\",\"provider\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].providerName) << L"\"}";
        }
        payload << L"],\"count\":" << mounts.size() << L"}";
        NfsOutputFormatter::PrintJsonEnvelope(L"mounts", ERROR_SUCCESS, payload.str());
        return ERROR_SUCCESS;
    }

    if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
        NfsOutputFormatter::PrintDelimitedRow({L"localDrive", L"remotePath", L"provider"}, options.outputMode);
        for (const auto& m : mounts) {
            NfsOutputFormatter::PrintDelimitedRow({m.localDrive, m.remotePath, m.providerName}, options.outputMode);
        }
        return ERROR_SUCCESS;
    }

    if (mounts.empty()) {
        NfsOutputFormatter::LogInfo(options, L"No active NFS mounts detected via WNet API.\n");
        return ERROR_SUCCESS;
    }

    wprintf(L"%-12s %-35s %s\n", L"Local Drive", L"Remote Path", L"Provider");
    wprintf(L"----------------------------------------------------------------------\n");

    for (const auto& m : mounts) {
        wprintf(L"%-12s %-35s %s\n", m.localDrive.empty() ? L"(N/A)" : m.localDrive.c_str(),
                m.remotePath.c_str(), m.providerName.c_str());
    }
    return ERROR_SUCCESS;
}

// --- NfsStatToolController ---
void NfsStatToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --nfsstat [--json]\n"
            L"Displays ClientForNFS registry configuration values (Timeout, Retries, MountType).\n");
}

DWORD NfsStatToolController::Execute(const GlobalOptions& options) {
    ULONGLONG startTick = GetTickCount64();
    DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
    if (stateErr != ERROR_SUCCESS) return stateErr;

    if (options.outputMode == OutputMode::Text) {
        NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Windows Client for NFS Status ]\n\n");
    }

    const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;
    DWORD timeout = 0, retries = 0, mountType = 0;

    DWORD r1 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueTimeout, &timeout);
    DWORD r2 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueRetries, &retries);
    DWORD r3 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueMountType, &mountType);

    if (options.outputMode == OutputMode::Json) {
        std::wstringstream payload;
        payload << L"{\"subKey\":\"" << NfsOutputFormatter::JsonEscape(subKey) << L"\",\"timeout\":";
        payload << ((r1 == ERROR_SUCCESS) ? std::to_wstring(timeout) : L"null");
        payload << L",\"retries\":" << ((r2 == ERROR_SUCCESS) ? std::to_wstring(retries) : L"null");
        payload << L",\"mountType\":" << ((r3 == ERROR_SUCCESS) ? std::to_wstring(mountType) : L"null");
        payload << L",\"mountTypeLabel\":";
        if (r3 == ERROR_SUCCESS) {
            payload << L"\"" << (mountType == 1 ? L"Hard" : L"Soft") << L"\"";
        } else {
            payload << L"null";
        }
        payload << L"}";
        DWORD status = (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
        NfsOutputFormatter::PrintJsonEnvelope(L"nfsstat", status, payload.str());
        return status;
    }

    if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
        NfsOutputFormatter::PrintDelimitedRow({L"key", L"value"}, options.outputMode);
        NfsOutputFormatter::PrintDelimitedRow({L"Timeout", (r1 == ERROR_SUCCESS) ? std::to_wstring(timeout) : L"N/A"}, options.outputMode);
        NfsOutputFormatter::PrintDelimitedRow({L"Retries", (r2 == ERROR_SUCCESS) ? std::to_wstring(retries) : L"N/A"}, options.outputMode);
        NfsOutputFormatter::PrintDelimitedRow({L"MountType", (r3 == ERROR_SUCCESS) ? std::to_wstring(mountType) : L"N/A"}, options.outputMode);
        NfsOutputFormatter::PrintDelimitedRow({L"MountTypeLabel", (r3 == ERROR_SUCCESS) ? (mountType == 1 ? L"Hard" : L"Soft") : L"N/A"}, options.outputMode);
        return (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
    }

    if (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) {
        wprintf(L"ClientForNFS Registry Configuration (%s):\n", subKey);
        wprintf(L"  Timeout (sec) : %s\n", (r1 == ERROR_SUCCESS) ? std::to_wstring(timeout).c_str() : L"N/A");
        wprintf(L"  Retries       : %s\n", (r2 == ERROR_SUCCESS) ? std::to_wstring(retries).c_str() : L"N/A");
        wprintf(L"  Mount Type    : %s\n", (r3 == ERROR_SUCCESS) ? (mountType == 1 ? L"Hard" : L"Soft") : L"N/A");
        return ERROR_SUCCESS;
    }

    NfsOutputFormatter::LogInfo(options, L"[!] Client for NFS Registry keys not found under HKLM\\%s.\n", subKey);
    return ERROR_FILE_NOT_FOUND;
}

// --- ExportFsToolController ---
void ExportFsToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --exportfs\n"
            L"Lists local ServerForNFS export keys and falls back to NetApi32 share enumeration when absent.\n");
}

DWORD ExportFsToolController::ToolExportFs(bool suppressOutput, std::wstring* jsonPayload) {
    struct ShareRecord {
        std::wstring name;
        std::wstring path;
    };

    if (!suppressOutput) {
        wprintf(L"[ nfsctl : Local Windows Server for NFS Exports ]\n\n");
    }

    const wchar_t* exportKey = NfsRegistry::kServerExportsSubKey;
    std::vector<std::wstring> subkeys;
    std::vector<ShareRecord> shares;
    NfsRegistryEngine::WinRegEnumSubkeys(HKEY_LOCAL_MACHINE, exportKey, subkeys);

    auto buildRegistryExportJson = [&](const std::vector<std::wstring>& keys, const std::vector<ShareRecord>& localShares,
                                       const std::wstring& source) -> std::wstring {
        std::wstringstream payload;
        payload << L"{\"exports\":[";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) payload << L",";
            std::wstring valuesJson;
            DWORD valueCount = 0;
            std::wstring fullKey = std::wstring(exportKey) + L"\\" + keys[i];
            DWORD valuesErr = NfsOutputFormatter::BuildRegistryValuesJson(HKEY_LOCAL_MACHINE, fullKey.c_str(), valuesJson, valueCount);
            payload << L"{\"id\":\"" << NfsOutputFormatter::JsonEscape(keys[i]) << L"\",\"valueCount\":" << valueCount
                    << L",\"values\":" << (valuesJson.empty() ? L"[]" : valuesJson);
            if (valuesErr != ERROR_SUCCESS) {
                payload << L",\"valueError\":" << valuesErr;
            }
            payload << L"}";
        }
        payload << L"],\"shares\":[";
        for (size_t i = 0; i < localShares.size(); ++i) {
            if (i > 0) payload << L",";
            payload << L"{\"name\":\"" << NfsOutputFormatter::JsonEscape(localShares[i].name)
                    << L"\",\"path\":\"" << NfsOutputFormatter::JsonEscape(localShares[i].path) << L"\"}";
        }
        payload << L"],\"source\":\"" << NfsOutputFormatter::JsonEscape(source) << L"\",\"count\":" << keys.size()
                << L",\"shareCount\":" << localShares.size() << L"}";
        return payload.str();
    };

    if (subkeys.empty()) {
        if (!suppressOutput) {
            wprintf(L"No local exports found under HKLM\\%s\n", exportKey);
            wprintf(L"NetApi32 Share Fallback: showing share entries instead.\n\n");
        }

        PSHARE_INFO_502 BufPtr = NULL;
        DWORD er = 0, tr = 0, resume = 0;
        NET_API_STATUS res = NetShareEnum(NULL, 502, reinterpret_cast<LPBYTE*>(&BufPtr), MAX_PREFERRED_LENGTH, &er, &tr, &resume);

        if (res == NERR_Success && BufPtr != NULL) {
            for (DWORD i = 0; i < er; ++i) {
                shares.push_back({BufPtr[i].shi502_netname != NULL ? BufPtr[i].shi502_netname : L"",
                                  BufPtr[i].shi502_path != NULL ? BufPtr[i].shi502_path : L""});
            }
            if (!suppressOutput) {
                wprintf(L"%-20s %s\n", L"NetApi Share Name", L"Local Path");
                wprintf(L"--------------------------------------------------\n");
                for (DWORD i = 0; i < er; ++i) {
                    wprintf(L"%-20s %s\n", BufPtr[i].shi502_netname, BufPtr[i].shi502_path);
                }
            }
            NetApiBufferFree(BufPtr);
            if (jsonPayload != nullptr) {
                *jsonPayload = buildRegistryExportJson(subkeys, shares, L"netapi");
            }
            return ERROR_SUCCESS;
        }
        if (jsonPayload != nullptr) {
            *jsonPayload = L"{\"exports\":[],\"shares\":[],\"source\":\"netapi\",\"count\":0,\"shareCount\":0}";
        }
        return res;
    }

    for (const auto& key : subkeys) {
        if (!suppressOutput) {
            std::wstring valuesJson;
            DWORD valueCount = 0;
            std::wstring fullKey = std::wstring(exportKey) + L"\\" + key;
            DWORD valuesErr = NfsOutputFormatter::BuildRegistryValuesJson(HKEY_LOCAL_MACHINE, fullKey.c_str(), valuesJson, valueCount);
            if (valuesErr == ERROR_SUCCESS) {
                wprintf(L"  Export Entry ID: %s (%lu registry value%s)\n",
                        key.c_str(), valueCount, (valueCount == 1) ? L"" : L"s");
            } else {
                wprintf(L"  Export Entry ID: %s (registry values unavailable)\n", key.c_str());
            }
        }
    }

    if (jsonPayload != nullptr) {
        *jsonPayload = buildRegistryExportJson(subkeys, shares, L"registry");
    }

    return ERROR_SUCCESS;
}

DWORD ExportFsToolController::Execute(const GlobalOptions& options) {
    if (options.outputMode == OutputMode::Json) {
        std::wstring payload;
        DWORD rc = ToolExportFs(true, &payload);
        NfsOutputFormatter::PrintJsonEnvelope(L"exportfs", rc, payload.empty() ? L"{\"exports\":[],\"shares\":[]}" : payload);
        return rc;
    }
    return ToolExportFs(false);
}

// --- ShowMountToolController ---
void ShowMountToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --showmount <host> [--nfsv3|--nfsv4|--nfsboth] [--timeout-ms <n>] [--ipv4|--ipv6|--dual-stack] [--json]\n"
            L"Probes NFSv3 via Portmapper/111 plus 2049, and NFSv4 via 2049 directly.\n");
}

DWORD ShowMountToolController::Execute(const wchar_t* remote_host, const GlobalOptions& options) {
    if (options.outputMode == OutputMode::Text) {
        NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Probing Remote NFS Server (%s) ]\n\n", remote_host);
    }
    NfsOutputFormatter::LogTrace(options, L"[trace] probing host=%s family=%s timeoutMs=%d nfsMode=%s\n", remote_host,
             NfsOutputFormatter::AddressFamilyLabel(options.addressFamily).c_str(), options.timeoutMs, NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode).c_str());

    BOOL nfs3_portmapper = FALSE;
    BOOL nfs3_port2049 = FALSE;
    BOOL nfs4_port2049 = FALSE;

    if (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V3) {
        nfs3_portmapper = NfsProbeEngine::ProbeTcpPort(remote_host, 111, options.timeoutMs, options.addressFamily);
        nfs3_port2049   = NfsProbeEngine::ProbeTcpPort(remote_host, 2049, options.timeoutMs, options.addressFamily);
    }
    if (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V4) {
        nfs4_port2049 = NfsProbeEngine::ProbeTcpPort(remote_host, 2049, options.timeoutMs, options.addressFamily);
    }

    BOOL nfs3_online = (nfs3_portmapper || nfs3_port2049);
    BOOL nfs4_online = nfs4_port2049;
    BOOL showV3 = (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V3);
    BOOL showV4 = (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V4);

    DWORD status = ((showV3 && nfs3_online) || (showV4 && nfs4_online)) ? ERROR_SUCCESS : ERROR_DEV_NOT_EXIST;

    if (options.outputMode == OutputMode::Json) {
        std::wstringstream payload;
        payload << L"{\"host\":\"" << NfsOutputFormatter::JsonEscape(remote_host)
                << L"\",\"family\":\"" << NfsOutputFormatter::JsonEscape(NfsOutputFormatter::AddressFamilyLabel(options.addressFamily))
                << L"\",\"nfsMode\":\"" << NfsOutputFormatter::JsonEscape(NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode))
                << L"\",\"timeoutMs\":" << options.timeoutMs
                << L",\"nfsv3\":{\"port111\":" << (nfs3_portmapper ? L"true" : L"false")
                << L",\"port2049\":" << (nfs3_port2049 ? L"true" : L"false")
                << L",\"online\":" << (nfs3_online ? L"true" : L"false") << L"}"
                << L",\"nfsv4\":{\"port2049\":" << (nfs4_port2049 ? L"true" : L"false")
                << L",\"online\":" << (nfs4_online ? L"true" : L"false") << L"}"
                << L"}";
        NfsOutputFormatter::PrintJsonEnvelope(L"showmount", status, payload.str());
        return status;
    }

    if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
        NfsOutputFormatter::PrintDelimitedRow({L"host", L"family", L"nfsMode", L"timeoutMs", L"nfsv3_port111", L"nfsv3_port2049", L"nfsv4_port2049", L"nfsv3_online", L"nfsv4_online"}, options.outputMode);
        NfsOutputFormatter::PrintDelimitedRow({remote_host,
                           NfsOutputFormatter::AddressFamilyLabel(options.addressFamily),
                           NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode),
                           std::to_wstring(options.timeoutMs),
                           nfs3_portmapper ? L"true" : L"false",
                           nfs3_port2049 ? L"true" : L"false",
                           nfs4_port2049 ? L"true" : L"false",
                           nfs3_online ? L"true" : L"false",
                           nfs4_online ? L"true" : L"false"},
                          options.outputMode);
        return status;
    }

    if (showV3) {
        NfsOutputFormatter::LogInfo(options, L"  NFSv3 Portmapper (TCP 111)  : %s\n", nfs3_portmapper ? L"ONLINE" : L"OFFLINE/BLOCKED");
        NfsOutputFormatter::LogInfo(options, L"  NFSv3 NFS        (TCP 2049) : %s\n", nfs3_port2049 ? L"ONLINE" : L"OFFLINE/BLOCKED");
    }
    if (showV4) {
        NfsOutputFormatter::LogInfo(options, L"  NFSv4 NFS        (TCP 2049) : %s\n", nfs4_port2049 ? L"ONLINE" : L"OFFLINE/BLOCKED");
    }
    NfsOutputFormatter::LogInfo(options, L"\n");

    return status;
}

// --- NfsConfToolController ---
void NfsConfToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --nfsconf --get <KeyName> [--json]\n"
            L"       nfsctl.exe --nfsconf --set <KeyName> <Value> [--confirm|--dry-run]\n");
}

DWORD NfsConfToolController::Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
    ULONGLONG startTick = GetTickCount64();
    DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
    if (stateErr != ERROR_SUCCESS) return stateErr;

    std::wstring keyName = L"", setValStr = L"";
    BOOL mode_get = FALSE, mode_set = FALSE;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == L"--get" && i + 1 < args.size()) {
            mode_get = TRUE;
            keyName = args[++i];
        }
        if (args[i] == L"--set" && i + 2 < args.size()) {
            mode_set = TRUE;
            keyName = args[++i];
            setValStr = args[++i];
        }
    }

    const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;

    if (mode_get) {
        DWORD val = 0;
        DWORD dwErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), &val);
        if (dwErr == ERROR_SUCCESS) {
            if (options.outputMode == OutputMode::Json) {
                std::wstringstream payload;
                payload << L"{\"key\":\"" << NfsOutputFormatter::JsonEscape(keyName) << L"\",\"value\":" << val << L"}";
                NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.get", ERROR_SUCCESS, payload.str());
            } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
                NfsOutputFormatter::PrintDelimitedRow({L"key", L"value"}, options.outputMode);
                NfsOutputFormatter::PrintDelimitedRow({keyName, std::to_wstring(val)}, options.outputMode);
            } else {
                NfsOutputFormatter::LogInfo(options, L"%s = %lu\n", keyName.c_str(), val);
            }
            return ERROR_SUCCESS;
        }
        if (options.outputMode == OutputMode::Json) {
            NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.get", dwErr, L"{\"key\":null,\"value\":null}",
                              L"Registry key not found");
        } else {
            fwprintf(stderr, L"Error: Key '%s' not found under HKLM\\%s\n", keyName.c_str(), subKey);
        }
        return dwErr;
    }

    if (mode_set) {
        bool shouldExecute = false;
        std::wstring dryRunMessage = L"[dry-run] Would set HKLM\\" + std::wstring(subKey) + L"\\" + keyName + L" = " + setValStr + L"\n";
        DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"nfsconf --set", dryRunMessage, shouldExecute);
        if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

        LONG dwordVal = 0;
        if (!NfsSecurityManager::SafeWstol(setValStr.c_str(), &dwordVal)) {
            fwprintf(stderr, L"Error: Value must be a valid integer.\n");
            return ERROR_INVALID_PARAMETER;
        }

        DWORD existingValue = 0;
        DWORD existingErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), &existingValue);
        if (existingErr == ERROR_SUCCESS && existingValue != static_cast<DWORD>(dwordVal) && !options.forceWrite) {
            fwprintf(stderr, L"Error: Existing value differs (%lu). Use --force to overwrite.\n", existingValue);
            return ERROR_CANCELLED;
        }

        DWORD backupErr = NfsRegistryEngine::BackupRegistryValueIfRequested(subKey, keyName.c_str(), existingValue, existingErr, options);
        if (backupErr != ERROR_SUCCESS) return backupErr;

        DWORD dwErr = NfsRegistryEngine::WinRegSetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), static_cast<DWORD>(dwordVal));
        if (dwErr == ERROR_SUCCESS) {
            if (options.outputMode == OutputMode::Json) {
                std::wstringstream payload;
                payload << L"{\"key\":\"" << NfsOutputFormatter::JsonEscape(keyName)
                        << L"\",\"oldValue\":" << ((existingErr == ERROR_SUCCESS) ? std::to_wstring(existingValue) : L"null")
                        << L",\"newValue\":" << dwordVal << L"}";
                NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.set", ERROR_SUCCESS, payload.str());
            } else {
                NfsOutputFormatter::LogInfo(options, L"[+] Successfully updated Registry: %s = %ld\n", keyName.c_str(), dwordVal);
            }
            return ERROR_SUCCESS;
        }
        NfsOutputFormatter::PrintWin32Error(L"WinRegSetDWORD failed", dwErr);
        return dwErr;
    }

    wprintf(L"Use '--get <KeyName>' or '--set <KeyName> <Value>' to modify ClientForNFS Registry.\n");
    return ERROR_SUCCESS;
}

// --- NfsIdMapToolController ---
void NfsIdMapToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --nfsidmap <AccountName>\n"
            L"Resolves a Windows account name to its SID.\n");
}

DWORD NfsIdMapToolController::Execute(const wchar_t* account_name, const GlobalOptions& options) {
    NfsIdMapResult resolved = NfsSecurityManager::ResolveAccountToSid(account_name);
    DWORD rc = resolved.code;
    if (options.outputMode == OutputMode::Json) {
        std::wstringstream payload;
        payload << L"{\"account\":\"" << NfsOutputFormatter::JsonEscape(resolved.account)
                << L"\",\"resolved\":" << ((rc == ERROR_SUCCESS) ? L"true" : L"false")
                << L",\"domain\":";
        if (rc == ERROR_SUCCESS) {
            payload << L"\"" << NfsOutputFormatter::JsonEscape(resolved.domain) << L"\"";
        } else {
            payload << L"null";
        }
        payload << L",\"sid\":";
        if (rc == ERROR_SUCCESS) {
            payload << L"\"" << NfsOutputFormatter::JsonEscape(resolved.sid) << L"\"";
        } else {
            payload << L"null";
        }
        payload << L"}";
        NfsOutputFormatter::PrintJsonEnvelope(L"nfsidmap", rc, payload.str());
    } else if (rc == ERROR_SUCCESS) {
        wprintf(L"Windows Account Resolution:\n");
        wprintf(L"  Account Name : %s\n", resolved.account.c_str());
        wprintf(L"  Domain       : %s\n", resolved.domain.c_str());
        wprintf(L"  SID          : %s\n", resolved.sid.c_str());
    } else {
        NfsOutputFormatter::PrintWin32Error(L"account resolution failed", rc);
    }
    return rc;
}

// --- RpcDebugToolController ---
void RpcDebugToolController::PrintHelp() {
    wprintf(L"USAGE: nfsctl.exe --rpcdebug [--json]\n"
            L"       nfsctl.exe --rpcdebug --set <bitmask> [--confirm|--dry-run]\n");
}

DWORD RpcDebugToolController::Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
    ULONGLONG startTick = GetTickCount64();
    DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
    if (stateErr != ERROR_SUCCESS) return stateErr;

    std::wstring setValStr = L"";
    BOOL mode_set = FALSE;

    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == L"--set" || args[i] == L"-s") && i + 1 < args.size()) {
            mode_set = TRUE;
            setValStr = args[++i];
        }
    }

    const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;

    if (mode_set) {
        bool shouldExecute = false;
        std::wstring dryRunMessage = L"[dry-run] Would set ClientForNFS DebugFlags = " + setValStr + L"\n";
        DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"rpcdebug --set", dryRunMessage, shouldExecute);
        if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

        LONG flags = 0;
        if (!NfsSecurityManager::SafeWstol(setValStr.c_str(), &flags)) {
            fwprintf(stderr, L"Error: Debug flag must be an integer bitmask.\n");
            return ERROR_INVALID_PARAMETER;
        }

        DWORD existingFlags = 0;
        DWORD existingErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, &existingFlags);
        if (existingErr == ERROR_SUCCESS && existingFlags != static_cast<DWORD>(flags) && !options.forceWrite) {
            fwprintf(stderr, L"Error: Existing DebugFlags differs (%lu). Use --force to overwrite.\n", existingFlags);
            return ERROR_CANCELLED;
        }

        DWORD backupErr = NfsRegistryEngine::BackupRegistryValueIfRequested(subKey, NfsRegistry::kValueDebugFlags, existingFlags, existingErr, options);
        if (backupErr != ERROR_SUCCESS) return backupErr;

        DWORD dwErr = NfsRegistryEngine::WinRegSetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, static_cast<DWORD>(flags));
        if (dwErr == ERROR_SUCCESS) {
            if (options.outputMode == OutputMode::Json) {
                std::wstringstream payload;
                payload << L"{\"oldValue\":" << ((existingErr == ERROR_SUCCESS) ? std::to_wstring(existingFlags) : L"null")
                        << L",\"newValue\":" << flags << L"}";
                NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.set", ERROR_SUCCESS, payload.str());
            } else {
                NfsOutputFormatter::LogInfo(options, L"[+] Set ClientForNFS DebugFlags = %ld\n", flags);
            }
            return ERROR_SUCCESS;
        }
        NfsOutputFormatter::PrintWin32Error(L"failed to set DebugFlags", dwErr);
        return dwErr;
    }

    DWORD debugFlags = 0;
    if (NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, &debugFlags) == ERROR_SUCCESS) {
        if (options.outputMode == OutputMode::Json) {
            std::wstringstream payload;
            payload << L"{\"debugFlags\":" << debugFlags << L"}";
            NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.get", ERROR_SUCCESS, payload.str());
        } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
            NfsOutputFormatter::PrintDelimitedRow({L"debugFlags"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({std::to_wstring(debugFlags)}, options.outputMode);
        } else {
            NfsOutputFormatter::LogInfo(options, L"ClientForNFS DebugFlags = %lu\n", debugFlags);
        }
    } else {
        if (options.outputMode == OutputMode::Json) {
            NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.get", ERROR_SUCCESS, L"{\"debugFlags\":0}");
        } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
            NfsOutputFormatter::PrintDelimitedRow({L"debugFlags"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({L"0"}, options.outputMode);
        } else {
            NfsOutputFormatter::LogInfo(options, L"ClientForNFS DebugFlags = 0 (Disabled / Default)\n");
        }
    }
    return ERROR_SUCCESS;
}
