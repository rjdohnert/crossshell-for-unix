#include "help_system.hpp"
#include "journal_engine.hpp"
#include "json.hpp"
#include "output_formatter.hpp"
#include "privilege_manager.hpp"
#include "reg_codec.hpp"
#include "registry_engine.hpp"
#include "registry_output_session.hpp"
#include "registry_pipe_buffer.hpp"
#include "registry_record.hpp"
#include "registryctl_app.hpp"
#include "scoped_transaction.hpp"
#include "shell_generator.hpp"
#include "string_conversion.hpp"
#include "undo_record.hpp"

int runRegistryctl(int argc, char* argv[]) {
    // 1. Force Windows Console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2. Enable SeBackupPrivilege and SeRestorePrivilege
    Security::EnableRequiredPrivileges();

    if (argc < 2) {
        HelpSystem::PrintGeneralHelp();
        return 1;
    }

    std::string command = argv[1];
    if (command == "help" || command == "--help" || command == "-h") {
        if (argc > 2) {
            HelpSystem::PrintTopicHelp(argv[2]);
        } else {
            HelpSystem::PrintGeneralHelp();
        }
        return 0;
    }

    if (command == "completions") {
        if (argc < 3) {
            std::cerr << "[-] Error: Specify target shell (powershell, zsh, ksh, cmd)\n";
            return 1;
        }
        std::string shell = argv[2];
        if (shell == "powershell" || shell == "pwsh") ShellGenerator::GeneratePowerShell();
        else if (shell == "zsh") ShellGenerator::GenerateZsh();
        else if (shell == "ksh") ShellGenerator::GenerateKsh();
        else if (shell == "cmd" || shell == "bat") ShellGenerator::GenerateCmdWrapper();
        else {
            std::cerr << "[-] Error: Unknown target shell: " << shell << "\n";
            return 1;
        }
        return 0;
    }

    // CLI Flag Parsing
    bool dryRun = false;
    bool useTx = false;
    bool jsonOutput = false;
    RegistryOutputFormat outputFormat = RegistryOutputFormat::Human;
    std::string pipeCommand;
    REGSAM viewSam = 0;
    std::string undoFile;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dry-run") dryRun = true;
        else if (arg == "--tx") useTx = true;
        else if (arg == "--json") jsonOutput = true;
        else if (arg == "--csv") outputFormat = RegistryOutputFormat::Csv;
        else if (arg == "--table") outputFormat = RegistryOutputFormat::Table;
        else if (arg == "--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else if (arg == "--undo-file" && i + 1 < argc) { undoFile = argv[++i]; }
        else if (arg.rfind("--undo-file=", 0) == 0) { undoFile = arg.substr(12); }
        else if (arg == "--view" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "32") viewSam = KEY_WOW64_32KEY;
            else if (v == "64") viewSam = KEY_WOW64_64KEY;
        }
        else if (arg.rfind("--view=", 0) == 0) {
            std::string v = arg.substr(7);
            if (v == "32") viewSam = KEY_WOW64_32KEY;
            else if (v == "64") viewSam = KEY_WOW64_64KEY;
        }
        else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        HelpSystem::PrintGeneralHelp();
        return 1;
    }

    command = positional[0];

    RegistryOutputSession outputSession(jsonOutput ? RegistryOutputFormat::Json : outputFormat, pipeCommand);

    // Transaction Management
    std::unique_ptr<ScopedTransaction> tx;
    if (useTx && !dryRun) {
        tx = std::make_unique<ScopedTransaction>();
        if (!tx->IsValid()) {
            std::cerr << "[-] Error: KTM Transaction initialization failed: " << Utils::FormatWin32Error(GetLastError()) << "\n";
            std::cerr << "[-] Hint: Verify KTM support in your Windows environment or omit --tx flag.\n";
            return 1;
        }
    }
    HANDLE hTx = tx ? tx->Get() : INVALID_HANDLE_VALUE;

    std::vector<UndoRecord> undoJournal;

    // ------------------------------------------------------------------------
    // DISPATCH: GET
    // ------------------------------------------------------------------------
    if (command == "get") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'get' requires <KeyPath> [ValueName]\n";
            return 1;
        }
        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(positional[1], root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Path: " << positional[1] << "\n";
            return 1;
        }
        std::string valueName = positional.size() > 2 ? positional[2] : "";
        RegistryRecord rec;
        if (!RegistryEngine::QueryValue(root, rootStr, subKey, valueName, rec, viewSam, hTx)) {
            if (jsonOutput) {
                std::cout << "{\"exists\": false, \"path\": \"" << positional[1] << "\"}\n";
            } else {
                std::cerr << "[-] Error: Value or Key not found.\n";
            }
            return 2;
        }

        if (jsonOutput) {
            OutputFormatter::PrintJson(rec);
        } else {
            std::cout << "Key   : " << rec.rootKey << "\\" << rec.subKey << "\n"
                      << "Value : " << (rec.valueName.empty() ? "(Default)" : rec.valueName) << "\n"
                      << "Type  : " << rec.GetTypeString() << "\n"
                      << "Data  : " << rec.GetFormattedData() << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: SET / DIFF
    // ------------------------------------------------------------------------
    if (command == "set" || command == "diff") {
        if (positional.size() < 5) {
            std::cerr << "[-] Error: Command requires: <Path> <ValueName> <Type> <Data>\n";
            std::cerr << "[-] Run 'registryctl help set' for syntax and examples.\n";
            return 1;
        }
        std::string path = positional[1];
        std::string valName = positional[2];
        std::string typeStr = positional[3];
        std::string rawDataStr = positional[4];

        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(path, root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Hive in: " << path << "\n";
            return 1;
        }

        DWORD dwType = RegCodec::ParseTypeString(typeStr);
        if (dwType == REG_NONE) {
            std::cerr << "[-] Error: Unsupported registry data type: " << typeStr << "\n";
            return 3;
        }

        std::vector<uint8_t> encoded;
        std::string parseErr;
        if (!RegCodec::EncodeData(dwType, rawDataStr, encoded, parseErr)) {
            std::cerr << "[-] Error encoding data: " << parseErr << "\n";
            return 3;
        }

        RegistryRecord oldRec;
        RegistryEngine::QueryValue(root, rootStr, subKey, valName, oldRec, viewSam, hTx);

        if (command == "diff" || dryRun) {
            OutputFormatter::PrintDiff(path, valName, oldRec, typeStr, rawDataStr);
            if (dryRun) std::cout << "[*] Dry-run enabled. No changes committed to disk.\n";
            return 0;
        }

        UndoRecord undo;
        undo.op = "set";
        undo.path = path;
        undo.valueName = valName;
        if (oldRec.exists) {
            undo.hadPreviousValue = true;
            undo.typeStr = oldRec.GetTypeString();
            undo.rawDataHex = Utils::BytesToHex(oldRec.rawData);
        } else {
            undo.hadPreviousValue = false;
        }
        undoJournal.push_back(undo);

        std::string writeErr;
        if (!RegistryEngine::SetValueRaw(root, subKey, valName, dwType, encoded, writeErr, viewSam, hTx)) {
            std::cerr << "[-] Write Error: " << writeErr << "\n";
            if (tx) tx->Rollback();
            return 1;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing KTM transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        if (jsonOutput) {
            std::cout << "{\"status\": \"success\", \"action\": \"set\", \"path\": \"" << path << "\"}\n";
        } else {
            std::cout << "[+] Successfully set " << path << " -> " << valName << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: DELETE
    // ------------------------------------------------------------------------
    if (command == "delete") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'delete' requires <Path> [ValueName]\n";
            return 1;
        }
        std::string path = positional[1];
        std::string valName = positional.size() > 2 ? positional[2] : "";

        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(path, root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Hive in: " << path << "\n";
            return 1;
        }

        RegistryRecord oldRec;
        RegistryEngine::QueryValue(root, rootStr, subKey, valName, oldRec, viewSam, hTx);

        if (dryRun) {
            OutputFormatter::PrintDiff(path, valName, oldRec, "", "", true);
            std::cout << "[*] Dry-run enabled. No deletions committed to disk.\n";
            return 0;
        }

        if (valName.empty()) {
            RegistryEngine::BackupKeyRecursive(root, rootStr, subKey, undoJournal, viewSam, hTx);
        } else if (oldRec.exists) {
            UndoRecord u;
            u.op = "set";
            u.path = path;
            u.valueName = valName;
            u.hadPreviousValue = true;
            u.typeStr = oldRec.GetTypeString();
            u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
            undoJournal.push_back(u);
        }

        std::string delErr;
        bool ok = valName.empty() ? 
            RegistryEngine::DeleteKeyRecursive(root, subKey, delErr, viewSam, hTx) : 
            RegistryEngine::DeleteValue(root, subKey, valName, delErr, viewSam, hTx);

        if (!ok) {
            std::cerr << "[-] Delete failed: " << delErr << "\n";
            if (tx) tx->Rollback();
            return 1;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        if (jsonOutput) {
            std::cout << "{\"status\": \"success\", \"action\": \"delete\", \"path\": \"" << path << "\"}\n";
        } else {
            std::cout << "[+] Successfully deleted: " << path << (valName.empty() ? "" : (" -> " + valName)) << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: BATCH
    // ------------------------------------------------------------------------
    if (command == "batch") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'batch' requires <Manifest.json>\n";
            return 1;
        }
        std::ifstream file(positional[1]);
        if (!file.is_open()) {
            std::cerr << "[-] Error: Could not open batch manifest file: " << positional[1] << "\n";
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Json::Parser parser(content);
        Json::Value rootJson = parser.ParseValue();
        if (!rootJson.IsObject() || !rootJson.HasKey("actions")) {
            std::cerr << "[-] Error: Invalid manifest format. Root object must contain 'actions' array.\n";
            return 3;
        }

        for (const auto& act : rootJson["actions"].AsArray()) {
            std::string op = act["op"].AsString();
            std::string path = act["path"].AsString();
            std::string val = act["value"].AsString();

            HKEY root = nullptr;
            std::string rootStr, subKey;
            if (!RegCodec::SplitPath(path, root, rootStr, subKey)) continue;

            RegistryRecord oldRec;
            RegistryEngine::QueryValue(root, rootStr, subKey, val, oldRec, viewSam, hTx);

            if (op == "set") {
                std::string typeStr = act["type"].AsString();
                std::string dataStr = act["data"].AsString();
                DWORD dwType = RegCodec::ParseTypeString(typeStr);
                std::vector<uint8_t> enc;
                std::string err;
                if (!RegCodec::EncodeData(dwType, dataStr, enc, err)) {
                    std::cerr << "[-] Batch item encode error: " << err << "\n";
                    if (tx) tx->Rollback();
                    return 3;
                }

                if (dryRun) {
                    OutputFormatter::PrintDiff(path, val, oldRec, typeStr, dataStr);
                    continue;
                }

                UndoRecord u;
                u.op = "set";
                u.path = path;
                u.valueName = val;
                u.hadPreviousValue = oldRec.exists;
                u.typeStr = oldRec.GetTypeString();
                u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
                undoJournal.push_back(u);

                std::string werr;
                if (!RegistryEngine::SetValueRaw(root, subKey, val, dwType, enc, werr, viewSam, hTx)) {
                    std::cerr << "[-] Batch write failed: " << werr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            } else if (op == "delete") {
                if (dryRun) {
                    OutputFormatter::PrintDiff(path, val, oldRec, "", "", true);
                    continue;
                }
                if (val.empty()) {
                    RegistryEngine::BackupKeyRecursive(root, rootStr, subKey, undoJournal, viewSam, hTx);
                } else if (oldRec.exists) {
                    UndoRecord u;
                    u.op = "set";
                    u.path = path;
                    u.valueName = val;
                    u.hadPreviousValue = true;
                    u.typeStr = oldRec.GetTypeString();
                    u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
                    undoJournal.push_back(u);
                }

                std::string derr;
                bool ok = val.empty() ?
                    RegistryEngine::DeleteKeyRecursive(root, subKey, derr, viewSam, hTx) :
                    RegistryEngine::DeleteValue(root, subKey, val, derr, viewSam, hTx);
                if (!ok) {
                    std::cerr << "[-] Batch delete failed: " << derr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            }
        }

        if (dryRun) {
            std::cout << "[*] Batch dry-run simulation complete. No changes applied.\n";
            return 0;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing batch transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        std::cout << "[+] Batch execution completed successfully.\n";
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: UNDO
    // ------------------------------------------------------------------------
    if (command == "undo") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'undo' requires <UndoLog.json>\n";
            return 1;
        }
        std::vector<UndoRecord> records;
        std::string err;
        if (!JournalEngine::LoadUndoLog(positional[1], records, err)) {
            std::cerr << "[-] " << err << "\n";
            return 1;
        }

        std::reverse(records.begin(), records.end());
        for (const auto& u : records) {
            HKEY root = nullptr;
            std::string rootStr, subKey;
            if (!RegCodec::SplitPath(u.path, root, rootStr, subKey)) continue;

            if (u.hadPreviousValue) {
                DWORD dwType = RegCodec::ParseTypeString(u.typeStr);
                std::vector<uint8_t> raw = Utils::HexToBytes(u.rawDataHex);
                if (dryRun) {
                    std::cout << "[*] Undo Preview: Restore " << u.path << " -> " << u.valueName << " [" << u.typeStr << "]\n";
                    continue;
                }
                std::string werr;
                if (!RegistryEngine::SetValueRaw(root, subKey, u.valueName, dwType, raw, werr, viewSam, hTx)) {
                    std::cerr << "[-] Undo restoration failed: " << werr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            } else {
                if (dryRun) {
                    std::cout << "[*] Undo Preview: Delete newly created " << u.path << " -> " << u.valueName << "\n";
                    continue;
                }
                std::string derr;
                RegistryEngine::DeleteValue(root, subKey, u.valueName, derr, viewSam, hTx);
            }
        }

        if (dryRun) {
            std::cout << "[*] Undo simulation complete. No changes made.\n";
            return 0;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing undo transaction.\n";
            return 1;
        }

        std::cout << "[+] Rollback completed successfully.\n";
        return 0;
    }

    std::cerr << "[-] Unknown command: " << command << ". Run 'registryctl help' for available commands.\n";
    return 1;
}
