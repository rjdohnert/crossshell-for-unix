#include "help_system.hpp"
#include "json.hpp"
#include "string_conversion.hpp"

void HelpSystem::PrintGeneralHelp() {
        std::cout << R"(registryctl(1)            CrossShell for UNIX Reference Manual                 registryctl(1)

    NAME
        registryctl - Windows Registry manipulation and automation utility

    SYNOPSIS
        registryctl <command> [ARGUMENTS...] [OPTIONS]

    DESCRIPTION
        registryctl provides sysadmins, DevOps engineers, and automated
        deployment pipelines with tools to safely query, mutate, batch-process,
        diff, and roll back Windows Registry edits with KTM transactional
        support.

    COMMANDS
        get <Path> [Value]
            Query registry value or verify existence.

        set <Path> <Value> <Type> <Data>
            Create or update a registry value atomically.

        delete <Path> [Value]
            Delete a value or entire recursive subkey.

        diff <Path> <Value> <Type> <Data>
            Compare target data against current value without modifying.

        batch <Manifest.json>
            Execute a JSON array of mutations atomically.

        undo <UndoLog.json>
            Replay an undo journal to restore exact prior state.

        completions <powershell|zsh|ksh|cmd>
            Emit shell completion and integration scripts.

        help [command|topic]
            Display detailed syntax, flags, and guides.

    OPTIONS
        --dry-run
            Simulate operations and display diffs without writing changes.

        --tx
            Execute modifications in an atomic Windows KTM transaction.

        --view <32|64>
            Force target WOW64 view (32-bit: WOW6432Node, 64-bit: native).

        --json
            Format standard output as JSON.

        --csv
            Format standard output as CSV.

        --table
            Format standard output as an ASCII table.

        --pipe <command>
            Send formatted output through the specified shell command.

        --undo-file <path>
            Emit an undo journal JSON to revert this operation later.

        -h, --help
            Display this reference manual and exit.

    EXAMPLES
        registryctl get HKLM\Software\MyApp Version
            Query a registry value.

        registryctl set HKLM\Software\MyApp Port REG_DWORD 8080 --tx
            Atomically set a DWORD value using KTM transactions.

        registryctl diff HKLM\Software\MyApp Port REG_DWORD 9090
            Preview changes between target and current value.

        registryctl delete HKLM\Software\OldApp --undo-file=undo.json
            Delete registry key recursively and save rollback journal.

        registryctl undo undo.json --tx
            Replay rollback journal inside a transaction.

    CrossShell for UNIX                                                          registryctl(1)
)";
    }

void HelpSystem::PrintTopicHelp(const std::string& topic) {
        std::string t = Utils::ToUpper(topic);
        if (t == "SET") {
            std::cout << R"(COMMAND: set
USAGE:
  registryctl set <KeyPath> <ValueName> <Type> <Data> [OPTIONS]

DESCRIPTION:
  Creates or updates a registry value. Missing parent keys are created
  automatically. To target the default (unnamed) key value, pass "" as ValueName.

DATA TYPES & FORMATS:
  REG_SZ          Standard Unicode string. Example: "Production Server"
  REG_EXPAND_SZ   Environment variable string. Example: "%SystemRoot%\System32"
  REG_DWORD       32-bit unsigned integer (decimal or 0x hex). Example: 8080 or 0x1F90
  REG_QWORD       64-bit unsigned integer (decimal or 0x hex). Example: 0x7FFFFFFFFFFFFFFF
  REG_MULTI_SZ    Multi-string list separated by semicolons. Example: "app1;app2;app3"
  REG_BINARY      Raw hex bytes with or without spaces. Example: "DEADBEEF0102"

EXAMPLES:
  registryctl set HKLM\Software\MyApp Version REG_SZ "3.4.1" --tx
  registryctl set HKLM\Software\MyApp Port REG_DWORD 8080 --undo-file=undo.json
  registryctl set HKLM\Software\MyApp Endpoints REG_MULTI_SZ "east;west;central"
  registryctl set HKLM\Software\MyApp Payload REG_BINARY "48656C6C6F" --view 64
)";
        } else if (t == "GET") {
            std::cout << R"(COMMAND: get
USAGE:
  registryctl get <KeyPath> [ValueName] [OPTIONS]

DESCRIPTION:
  Queries a registry value and displays its type, formatted data, and hex
  representation. If ValueName is omitted or passed as "", queries (Default).

EXAMPLES:
  registryctl get HKLM\Software\MyApp Version
  registryctl get HKLM\Software\MyApp Version --json
  registryctl get HKLM\Software\MyApp Version --view 32
)";
        } else if (t == "DELETE") {
            std::cout << R"(COMMAND: delete
USAGE:
  registryctl delete <KeyPath> [ValueName] [OPTIONS]

DESCRIPTION:
  Deletes a specific registry value, or if ValueName is omitted, recursively
  deletes the entire subkey hierarchy. When --undo-file is specified with a key
  deletion, the entire subtree of subkeys and values is captured recursively.

EXAMPLES:
  registryctl delete HKLM\Software\MyApp StaleValue --tx
  registryctl delete HKLM\Software\OldApp --undo-file=backup_tree.json
  registryctl delete HKLM\Software\OldApp --dry-run
)";
        } else if (t == "BATCH" || t == "BATCH-SCHEMA") {
            std::cout << R"(COMMAND: batch
USAGE:
  registryctl batch <Manifest.json> [OPTIONS]

DESCRIPTION:
  Executes a batch of mutations defined in a JSON manifest. If --tx is set,
  all operations succeed together or roll back completely on failure.

MANIFEST SCHEMA EXAMPLE:
  {
    "actions": [
      {
        "op": "set",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "MaxThreads",
        "type": "REG_DWORD",
        "data": "128"
      },
      {
        "op": "set",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "DatabaseUrl",
        "type": "REG_SZ",
        "data": "postgres://localhost:5432/db"
      },
      {
        "op": "delete",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "LegacyTimeout"
      }
    ]
  }

EXAMPLES:
  registryctl batch manifest.json --tx --undo-file=rollback.json
  registryctl batch manifest.json --dry-run
)";
        } else if (t == "UNDO") {
            std::cout << R"(COMMAND: undo
USAGE:
  registryctl undo <UndoLog.json> [OPTIONS]

DESCRIPTION:
  Restores prior registry state from an undo journal generated by a previous
  'set', 'delete', or 'batch' operation. Replays records in reverse order using
  exact byte-level hex restoration.

EXAMPLES:
  registryctl undo rollback.json --tx
  registryctl undo rollback.json --dry-run
)";
        } else if (t == "TRANSACTIONS") {
            std::cout << R"(TOPIC: transactions (--tx)
DESCRIPTION:
  When --tx is specified, registryctl initializes a Windows Kernel Transaction
  Manager (KTM) handle. All key creations, value writes, and deletions occur
  inside this transaction. If any error occurs or the process is interrupted,
  the transaction is automatically aborted by the Windows kernel.

NOTE:
  KTM/TxR is available on Windows 10/11 and Windows Server editions. In minimal
  containers or stripped WinPE images where TxR is disabled, omit --tx to use
  conventional hardened writes.
)";
        } else if (t == "WOW64" || t == "VIEW") {
            std::cout << R"(TOPIC: WOW64 & Architecture Views (--view)
DESCRIPTION:
  On 64-bit Windows, 32-bit applications are redirected to HKLM\Software\WOW6432Node.
  registryctl allows explicit view selection:
    --view 64      Force 64-bit native registry view (KEY_WOW64_64KEY)
    --view 32      Force 32-bit redirected view (KEY_WOW64_32KEY)
    (default)      Native OS architecture view
)";
        } else if (t == "EXAMPLES") {
            std::cout << R"(TOPIC: Production Workflow Examples

1. Safe Mutation with Dry-Run and Undo Journal:
   registryctl diff HKLM\Software\Engine CacheSize REG_DWORD 4096
   registryctl set HKLM\Software\Engine CacheSize REG_DWORD 4096 --tx --undo-file=undo.json

2. Automation Pipeline with JSON Output:
   registryctl get HKLM\Software\Engine CacheSize --json

3. Atomic Batch Deployment with Rollback on Failure:
   registryctl batch deploy.json --tx --undo-file=revert.json

4. Emergency Rollback:
   registryctl undo revert.json --tx
)";
        } else {
            PrintGeneralHelp();
        }
    }
