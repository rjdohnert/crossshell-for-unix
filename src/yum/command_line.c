#include "command_line.h"
#include "command_line_escaping.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"
#include "windows_update_query.h"
#include "privilege_check.h"
#include "windows_update_install.h"
#include "package_install.h"
#include "package_remove.h"
#include "package_search.h"
#include "package_info.h"
#include "package_list.h"
#include "package_upgrade.h"
#include "package_clean.h"
#include "repository_list.h"
#include "repository_add.h"
#include "repository_remove.h"
#include "help.h"

int Yum_Main(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        Yum_ShowHelp();
        return 1;
    }

    YumContext ctx = { .assume_yes = false, .verbose = false, .quiet = false };
    const wchar_t* command = NULL;
    const wchar_t* targets[MAX_CMDLINE / 16];
    int targetCount = 0;

    for (int i = 1; i < argc; ++i) {
        const wchar_t* arg = argv[i];
        if (wcscmp(arg, L"-y") == 0 || wcscmp(arg, L"--assumeyes") == 0) {
            ctx.assume_yes = true;
        } else if (wcscmp(arg, L"-v") == 0 || wcscmp(arg, L"--verbose") == 0) {
            ctx.verbose = true;
        } else if (wcscmp(arg, L"-q") == 0 || wcscmp(arg, L"--quiet") == 0) {
            ctx.quiet = true;
        } else if (wcscmp(arg, L"-h") == 0 || wcscmp(arg, L"--help") == 0) {
            Yum_ShowHelp();
            return 0;
        } else if (wcscmp(arg, L"--version") == 0) {
            wprintf(L"%ls\n", YUM_VERSION);
            return 0;
        } else if (arg[0] == L'-') {
            fwprintf(stderr, L"Unknown option: %ls\n", arg);
            Yum_ShowHelp();
            return 1;
        } else {
            if (!command) {
                command = arg;
            } else {
                if (targetCount < (int)(sizeof(targets) / sizeof(targets[0]))) {
                    targets[targetCount++] = arg;
                }
            }
        }
    }

    if (!command) {
        Yum_ShowHelp();
        return 0;
    }

    if (wcscmp(command, L"install") == 0) {
        return Yum_Install(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"localinstall") == 0) {
        return Yum_LocalInstall(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"localupdate") == 0) {
        return Yum_RpmInstall(&ctx, targets, targetCount, true);
    } else if (wcscmp(command, L"reinstall") == 0) {
        return Yum_Reinstall(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"downgrade") == 0) {
        return Yum_Downgrade(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"remove") == 0 || wcscmp(command, L"erase") == 0) {
        return Yum_Remove(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"search") == 0) {
        return Yum_Search(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"info") == 0) {
        return Yum_Info(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"list") == 0) {
        return Yum_List(&ctx, targetCount > 0 ? targets[0] : NULL);
    } else if (wcscmp(command, L"clean") == 0) {
        return Yum_Clean(&ctx, targetCount > 0 ? targets[0] : L"all");
    } else if (wcscmp(command, L"check-update") == 0 || wcscmp(command, L"check-upgrade") == 0) {
        return Yum_CheckUpdate(&ctx);
    } else if (wcscmp(command, L"update") == 0) {
        return Yum_UpdateWindows(&ctx);
    } else if (wcscmp(command, L"sys-upgrade") == 0) {
        return Yum_SystemUpgrade(&ctx);
    } else if (wcscmp(command, L"upgrade") == 0) {
        return Yum_Upgrade(&ctx, targets, targetCount);
    } else if (wcscmp(command, L"repolist") == 0) {
        return Yum_RepoList(&ctx);
    } else if (wcscmp(command, L"repo") == 0) {
        if (targetCount < 1) {
            fwprintf(stderr, L"Error: Subcommand missing (add/remove/list).\n");
            return 1;
        }
        const wchar_t* subCmd = targets[0];
        if (wcscmp(subCmd, L"add") == 0) {
            if (targetCount < 3) {
                fwprintf(stderr, L"Usage: yum repo add <name> <url>\n");
                return 1;
            }
            return Yum_RepoAdd(&ctx, targets[1], targets[2]);
        } else if (wcscmp(subCmd, L"remove") == 0 || wcscmp(subCmd, L"rm") == 0) {
            if (targetCount < 2) {
                fwprintf(stderr, L"Usage: yum repo remove <name>\n");
                return 1;
            }
            return Yum_RepoRemove(&ctx, targets[1]);
        } else if (wcscmp(subCmd, L"list") == 0) {
            return Yum_RepoList(&ctx);
        }
    } else if (wcscmp(command, L"help") == 0) {
        Yum_ShowHelp();
        return 0;
    }

    fwprintf(stderr, L"No such command: %ls. See 'yum help'.\n", command);
    return 1;
}
