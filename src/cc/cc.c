/**
 * ============================================================================
 * Program     : cc (Intelligent Clang/GCC compiler wrapper for Windows NT)
 * CrossShell for UNIX
 * Copyright 2026 PC/OpenSystems LLC.  All Rights Reserved
 * Licensed under the terms of the BSD 3-Clause License 
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#pragma comment(lib, "advapi32.lib")

#define CC_VERSION "1.2.0 (winNT-x86_64)"
#define MAX_CMD_LEN 32768

/* ============================================================================
 * ENUMS & FORWARD DECLARATIONS
 * ============================================================================ */
typedef enum {
    COMPILER_AUTO,
    COMPILER_CLANG,
    COMPILER_GCC
} CompilerPreference;

/* ============================================================================
 * CLASS: CcConfig
 * Encapsulates argument parsing, wrapper options, and passthrough flags.
 * ============================================================================ */
typedef struct CcConfig CcConfig;

struct CcConfig {
    /* Fields */
    CompilerPreference preference;
    bool  verbose;
    bool  show_path_only;
    bool  dry_run;
    char* explicit_compiler_path;

    char** compiler_args;
    int    arg_count;
    int    arg_capacity;

    /* Methods */
    bool (*Parse)(CcConfig* this, int argc, char** argv);
    void (*AddArg)(CcConfig* this, const char* arg);
    void (*PrintHelp)(const CcConfig* this);
    void (*PrintVersion)(const CcConfig* this);
};

static void CcConfig_AddArg(CcConfig* this, const char* arg) {
    if (this->arg_count >= this->arg_capacity) {
        this->arg_capacity *= 2;
        this->compiler_args = (char**)realloc(this->compiler_args, sizeof(char*) * this->arg_capacity);
    }
    this->compiler_args[this->arg_count++] = _strdup(arg);
}

static void CcConfig_PrintVersion(const CcConfig* this) {
    (void)this;
    printf("cc wrapper version %s\n", CC_VERSION);
    printf("Target Platform: Windows (Native Win32 Subsystem)\n");
}

static void CcConfig_PrintHelp(const CcConfig* this) {
    (void)this;
    printf(
        "Usage: cc [wrapper-options] [compiler-options] files...\n\n"
        "Description:\n"
        "  Universal C/C++ compiler wrapper for Windows. Detects and delegates to\n"
        "  Clang or GCC automatically, even if they are NOT in the system PATH.\n\n"
        "Wrapper Specific Options:\n"
        "  --cc-compiler=<target>   Select compiler backend ('clang', 'gcc', or 'auto')\n"
        "                           Default: 'auto' (prefers Clang, falls back to GCC)\n"
        "  --cc-show-path           Print the detected compiler binary path and exit\n"
        "  --cc-use-path=<path>     Force use of a specific compiler binary path\n"
        "  --cc-verbose             Print wrapper diagnostics and executed command line\n"
        "  --cc-dry-run             Locate compiler and print command without executing\n"
        "  --cc-help, -help         Display this comprehensive help manual and exit\n"
        "  --cc-version             Show wrapper version information and exit\n\n"
        "Compiler Search Locations (When not in system PATH):\n"
        "  * Visual Studio LLVM Toolset (%%ProgramFiles%%/Microsoft Visual Studio/...)\n"
        "  * Standalone LLVM Windows Installation (%%ProgramFiles%%/LLVM/bin)\n"
        "  * MSYS2 Subsystems (C:\\msys64\\{clang64, ucrt64, mingw64}\\bin)\n"
        "  * WinLibs Standalone Distributions (C:\\winlibs\\bin, C:\\mingw64\\bin)\n"
        "  * User Scoop Shims (%%USERPROFILE%%\\scoop\\apps\\{llvm,gcc}\\current\\bin)\n"
        "  * Chocolatey Tool Paths (%%ProgramData%%\\chocolatey\\bin)\n"
        "  * Legacy MinGW & TDM-GCC (C:\\MinGW\\bin, C:\\TDM-GCC-64\\bin)\n"
        "  * Windows Registry Application Paths (HKLM & HKCU)\n\n"
        "All other options (e.g. -Wall, -O2, -I, -L, -c, -o) are forwarded directly\n"
        "to the underlying backend compiler.\n"
    );
}

static bool CcConfig_Parse(CcConfig* this, int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if (strcmp(arg, "--cc-help") == 0 || strcmp(arg, "-help") == 0) {
            this->PrintHelp(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--cc-version") == 0) {
            this->PrintVersion(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--cc-verbose") == 0) {
            this->verbose = true;
        } else if (strcmp(arg, "--cc-show-path") == 0) {
            this->show_path_only = true;
        } else if (strcmp(arg, "--cc-dry-run") == 0) {
            this->dry_run = true;
        } else if (strncmp(arg, "--cc-compiler=", 14) == 0) {
            char* val = arg + 14;
            if (_stricmp(val, "clang") == 0) {
                this->preference = COMPILER_CLANG;
            } else if (_stricmp(val, "gcc") == 0) {
                this->preference = COMPILER_GCC;
            } else if (_stricmp(val, "auto") == 0) {
                this->preference = COMPILER_AUTO;
            } else {
                fprintf(stderr, "cc: error: unknown compiler backend '%s'\n", val);
                return false;
            }
        } else if (strncmp(arg, "--cc-use-path=", 14) == 0) {
            free(this->explicit_compiler_path);
            this->explicit_compiler_path = _strdup(arg + 14);
        } else {
            /* Standard compiler argument to be forwarded */
            this->AddArg(this, arg);
        }
    }
    return true;
}

CcConfig* New_CcConfig(void) {
    CcConfig* cfg = (CcConfig*)malloc(sizeof(CcConfig));
    cfg->preference = COMPILER_AUTO;
    cfg->verbose = false;
    cfg->show_path_only = false;
    cfg->dry_run = false;
    cfg->explicit_compiler_path = NULL;

    cfg->arg_count = 0;
    cfg->arg_capacity = 32;
    cfg->compiler_args = (char**)malloc(sizeof(char*) * cfg->arg_capacity);

    cfg->Parse        = CcConfig_Parse;
    cfg->AddArg       = CcConfig_AddArg;
    cfg->PrintHelp    = CcConfig_PrintHelp;
    cfg->PrintVersion = CcConfig_PrintVersion;
    return cfg;
}

void Delete_CcConfig(CcConfig* this) {
    if (!this) return;
    free(this->explicit_compiler_path);
    for (int i = 0; i < this->arg_count; i++) {
        free(this->compiler_args[i]);
    }
    free(this->compiler_args);
    free(this);
}

/* ============================================================================
 * CLASS: CompilerLocator
 * Scans directories, environments, and registry keys to locate Clang and GCC.
 * ============================================================================ */
typedef struct CompilerLocator CompilerLocator;

struct CompilerLocator {
    /* Fields */
    bool verbose;

    /* Methods */
    bool  (*FileExists)(const CompilerLocator* this, const char* path);
    char* (*ProbeRegistry)(const CompilerLocator* this, HKEY rootKey, const char* subKey, const char* valName);
    char* (*SearchPathEnv)(const CompilerLocator* this, const char* exeName);
    char* (*FindClang)(const CompilerLocator* this);
    char* (*FindGcc)(const CompilerLocator* this);
    char* (*Locate)(const CompilerLocator* this, CompilerPreference pref);
};

static bool CompilerLocator_FileExists(const CompilerLocator* this, const char* path) {
    (void)this;
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
}

/* Safe getenv: returns a newly-allocated copy (caller frees) or NULL. */
static char* GetEnvCopy(const char* name) {
    char* val = NULL;
    if (_dupenv_s(&val, NULL, name) != 0) return NULL;
    return val;
}

static char* CompilerLocator_ProbeRegistry(const CompilerLocator* this, HKEY rootKey, const char* subKey, const char* valName) {
    (void)this;
    HKEY hKey;
    char buffer[MAX_PATH];
    DWORD bufferSize = sizeof(buffer);

    if (RegOpenKeyExA(rootKey, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, valName, NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return _strdup(buffer);
        }
        RegCloseKey(hKey);
    }
    return NULL;
}

static char* CompilerLocator_SearchPathEnv(const CompilerLocator* this, const char* exeName) {
    (void)this;
    char resolved[MAX_PATH];
    DWORD result = SearchPathA(NULL, exeName, ".exe", MAX_PATH, resolved, NULL);
    if (result > 0 && result < MAX_PATH) {
        return _strdup(resolved);
    }
    return NULL;
}

static char* CompilerLocator_FindClang(const CompilerLocator* this) {
    /* 1. Standard PATH check */
    char* path = this->SearchPathEnv(this, "clang");
    if (path) return path;

    /* 2. Environment Variables */
    const char* envVars[] = { "LLVM_HOME", "CLANG_HOME", "LLVM_PATH" };
    for (size_t i = 0; i < sizeof(envVars)/sizeof(envVars[0]); i++) {
        char* envVal = GetEnvCopy(envVars[i]);
        if (envVal) {
            char candidate[MAX_PATH];
            snprintf(candidate, sizeof(candidate), "%s\\bin\\clang.exe", envVal);
            free(envVal);
            if (this->FileExists(this, candidate)) return _strdup(candidate);
        }
    }

    /* 3. Windows Registry Keys (HKLM, HKCU, and 32-bit-on-64-bit view) */
    const struct { HKEY root; const char* sub; } regKeys[] = {
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\LLVM\\LLVM" },
        { HKEY_CURRENT_USER,  "SOFTWARE\\LLVM\\LLVM" },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\LLVM\\LLVM" },
    };
    for (size_t i = 0; i < sizeof(regKeys)/sizeof(regKeys[0]); i++) {
        char* regPath = this->ProbeRegistry(this, regKeys[i].root, regKeys[i].sub, "");
        if (regPath) {
            char candidate[MAX_PATH];
            snprintf(candidate, sizeof(candidate), "%s\\bin\\clang.exe", regPath);
            free(regPath);
            if (this->FileExists(this, candidate)) return _strdup(candidate);
        }
    }

    /* 4. Common Out-of-PATH Installation Locations */
    char progFiles[MAX_PATH], progFilesX86[MAX_PATH], userProfile[MAX_PATH];
    GetEnvironmentVariableA("ProgramFiles", progFiles, MAX_PATH);
    GetEnvironmentVariableA("ProgramFiles(x86)", progFilesX86, MAX_PATH);
    GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);

    const char* knownTargets[12];
    char t0[MAX_PATH], t1[MAX_PATH], t5[MAX_PATH], t7[MAX_PATH],
         t8[MAX_PATH], t9[MAX_PATH], t10[MAX_PATH];
    snprintf(t0,  MAX_PATH, "%s\\LLVM\\bin\\clang.exe", progFiles);
    snprintf(t1,  MAX_PATH, "%s\\LLVM\\bin\\clang.exe", progFilesX86);
    snprintf(t5,  MAX_PATH, "%s\\scoop\\apps\\llvm\\current\\bin\\clang.exe", userProfile);
    snprintf(t7,  MAX_PATH, "%s\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\Llvm\\bin\\clang.exe", progFiles);
    snprintf(t8,  MAX_PATH, "%s\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\bin\\clang.exe", progFiles);
    snprintf(t9,  MAX_PATH, "%s\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\Llvm\\bin\\clang.exe", progFiles);
    snprintf(t10, MAX_PATH, "%s\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\Llvm\\bin\\clang.exe", progFiles);
    knownTargets[0] = t0;
    knownTargets[1] = t1;
    knownTargets[2] = "C:\\msys64\\clang64\\bin\\clang.exe";
    knownTargets[3] = "C:\\msys64\\ucrt64\\bin\\clang.exe";
    knownTargets[4] = "C:\\msys64\\mingw64\\bin\\clang.exe";
    knownTargets[5] = t5;
    knownTargets[6] = "C:\\ProgramData\\chocolatey\\bin\\clang.exe";
    knownTargets[7] = t7;
    knownTargets[8] = t8;
    knownTargets[9] = t9;
    knownTargets[10] = t10;
    knownTargets[11] = "C:\\mingw64\\bin\\clang.exe";

    for (int i = 0; i < 12; i++) {
        if (this->FileExists(this, knownTargets[i])) {
            return _strdup(knownTargets[i]);
        }
    }

    return NULL;
}

static char* CompilerLocator_FindGcc(const CompilerLocator* this) {
    /* 1. Standard PATH check */
    char* path = this->SearchPathEnv(this, "gcc");
    if (path) return path;

    /* 2. Environment Variables */
    const char* envVars[] = { "MINGW_HOME", "GCC_HOME", "MSYS2_HOME" };
    for (size_t i = 0; i < sizeof(envVars)/sizeof(envVars[0]); i++) {
        char* envVal = GetEnvCopy(envVars[i]);
        if (envVal) {
            char candidate[MAX_PATH];
            snprintf(candidate, sizeof(candidate), "%s\\bin\\gcc.exe", envVal);
            free(envVal);
            if (this->FileExists(this, candidate)) return _strdup(candidate);
        }
    }

    /* 3. Common Out-of-PATH Installation Locations */
    char userProfile[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);

    const char* knownTargets[10];
    char t7[MAX_PATH], t8[MAX_PATH];
    snprintf(t7, MAX_PATH, "%s\\scoop\\apps\\gcc\\current\\bin\\gcc.exe", userProfile);
    snprintf(t8, MAX_PATH, "%s\\scoop\\apps\\mingw\\current\\bin\\gcc.exe", userProfile);
    knownTargets[0] = "C:\\msys64\\ucrt64\\bin\\gcc.exe";
    knownTargets[1] = "C:\\msys64\\mingw64\\bin\\gcc.exe";
    knownTargets[2] = "C:\\msys64\\usr\\bin\\gcc.exe";
    knownTargets[3] = "C:\\winlibs\\bin\\gcc.exe";
    knownTargets[4] = "C:\\mingw64\\bin\\gcc.exe";
    knownTargets[5] = "C:\\MinGW\\bin\\gcc.exe";
    knownTargets[6] = "C:\\TDM-GCC-64\\bin\\gcc.exe";
    knownTargets[7] = t7;
    knownTargets[8] = t8;
    knownTargets[9] = "C:\\ProgramData\\chocolatey\\bin\\gcc.exe";

    for (int i = 0; i < 10; i++) {
        if (this->FileExists(this, knownTargets[i])) {
            return _strdup(knownTargets[i]);
        }
    }

    return NULL;
}

static char* CompilerLocator_Locate(const CompilerLocator* this, CompilerPreference pref) {
    char* resolved = NULL;

    if (pref == COMPILER_CLANG) {
        resolved = this->FindClang(this);
    } else if (pref == COMPILER_GCC) {
        resolved = this->FindGcc(this);
    } else {
        /* Auto mode: Prefer Clang, fallback to GCC */
        resolved = this->FindClang(this);
        if (!resolved) {
            resolved = this->FindGcc(this);
        }
    }
    return resolved;
}

CompilerLocator* New_CompilerLocator(bool verbose) {
    CompilerLocator* loc = (CompilerLocator*)malloc(sizeof(CompilerLocator));
    loc->verbose = verbose;

    loc->FileExists    = CompilerLocator_FileExists;
    loc->ProbeRegistry = CompilerLocator_ProbeRegistry;
    loc->SearchPathEnv = CompilerLocator_SearchPathEnv;
    loc->FindClang     = CompilerLocator_FindClang;
    loc->FindGcc       = CompilerLocator_FindGcc;
    loc->Locate        = CompilerLocator_Locate;
    return loc;
}

void Delete_CompilerLocator(CompilerLocator* this) {
    if (this) free(this);
}

/* ============================================================================
 * CLASS: ProcessRunner
 * Builds quoting-safe argument strings and spawns the Win32 compiler process.
 * ============================================================================ */
typedef struct ProcessRunner ProcessRunner;

struct ProcessRunner {
    /* Fields */
    const char* binary_path;
    char**      args;
    int         arg_count;
    bool        verbose;

    /* Methods */
    char* (*BuildCommandLine)(const ProcessRunner* this);
    int   (*Run)(const ProcessRunner* this);
};

/* Append a single argument to a dynamically-grown buffer with proper
 * Windows CommandLineToArgvW quoting: backslash-escape backslashes that
 * precede a quote, escape embedded quotes, and wrap in quotes when the
 * argument contains spaces, tabs, or is empty. Returns false on overflow. */
static bool AppendArgQuoted(char** buf, size_t* len, size_t* cap, const char* arg) {
    bool needsQuotes = (arg[0] == '\0') || strchr(arg, ' ') || strchr(arg, '\t') || strchr(arg, '"');

    /* Worst case: every char needs escaping + surrounding quotes + space. */
    size_t need = strlen(arg) * 2 + 4;
    if (*len + need + 1 > *cap) {
        size_t newCap = *cap;
        while (*len + need + 1 > newCap) newCap *= 2;
        char* nb = (char*)realloc(*buf, newCap);
        if (!nb) return false;
        *buf = nb;
        *cap = newCap;
    }

    char* out = *buf + *len;
    if (*len > 0) *out++ = ' ';           /* separator */
    if (needsQuotes) *out++ = '"';

    size_t backslashes = 0;
    for (const char* p = arg; *p; p++) {
        if (*p == '\\') {
            backslashes++;
        } else if (*p == '"') {
            while (backslashes--) *out++ = '\\';  /* double preceding backslashes */
            backslashes = 0;
            *out++ = '\\';                         /* escape the quote */
            *out++ = '"';
        } else {
            while (backslashes--) *out++ = '\\';  /* flush literal backslashes */
            backslashes = 0;
            *out++ = *p;
        }
    }
    if (needsQuotes) {
        while (backslashes--) *out++ = '\\';      /* double trailing backslashes */
        *out++ = '"';
    } else {
        while (backslashes--) *out++ = '\\';      /* flush literal backslashes */
    }
    *out = '\0';
    *len = (size_t)(out - *buf);
    return true;
}

static char* ProcessRunner_BuildCommandLine(const ProcessRunner* this) {
    size_t cap = 4096, len = 0;
    char* cmd = (char*)malloc(cap);
    if (!cmd) return NULL;
    cmd[0] = '\0';

    if (!AppendArgQuoted(&cmd, &len, &cap, this->binary_path)) { free(cmd); return NULL; }
    for (int i = 0; i < this->arg_count; i++) {
        if (!AppendArgQuoted(&cmd, &len, &cap, this->args[i])) { free(cmd); return NULL; }
    }
    return cmd;
}

/* Spill compiler args (not the binary path) to a response file, returning
 * a newly-allocated command line of the form: "binary" "@file". */
static char* SpillToResponseFile(const ProcessRunner* this) {
    char tmpPath[MAX_PATH], tmpFile[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmpPath)) return NULL;
    if (!GetTempFileNameA(tmpPath, "cc", 0, tmpFile)) return NULL;

    FILE* f = NULL;
    if (fopen_s(&f, tmpFile, "w") != 0 || !f) return NULL;
    for (int i = 0; i < this->arg_count; i++) {
        /* Response files use the same quoting rules; write each arg quoted. */
        fputc('"', f);
        for (const char* p = this->args[i]; *p; p++) {
            if (*p == '"' || *p == '\\') fputc('\\', f);
            fputc(*p, f);
        }
        fputs("\" ", f);
    }
    fclose(f);

    size_t need = strlen(this->binary_path) + strlen(tmpFile) + 8;
    char* cmd = (char*)malloc(need);
    if (cmd) snprintf(cmd, need, "\"%s\" @\"%s\"", this->binary_path, tmpFile);
    return cmd;
}

static int ProcessRunner_Run(const ProcessRunner* this) {
    char* cmdLine = this->BuildCommandLine(this);
    if (!cmdLine) {
        fprintf(stderr, "cc: error: failed to build command line (out of memory)\n");
        return -1;
    }

    /* Win32 CreateProcess command-line limit is 32767 chars; spill to a
     * response file when we approach it. */
    if (strlen(cmdLine) > 32000) {
        if (this->verbose) {
            printf("[cc:info] command line too long (%zu chars), using response file\n",
                   strlen(cmdLine));
        }
        free(cmdLine);
        cmdLine = SpillToResponseFile(this);
        if (!cmdLine) {
            fprintf(stderr, "cc: error: command line too long and response file failed\n");
            return -1;
        }
    }

    if (this->verbose) {
        printf("[cc:exec] %s\n", cmdLine);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    /* Create process inheriting current console handles (stdin/stdout/stderr) */
    BOOL success = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    free(cmdLine);

    if (!success) {
        fprintf(stderr, "cc: error: failed to launch process (Win32 Error: %lu)\n", GetLastError());
        return -1;
    }

    /* Wait for child process to finish */
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}

ProcessRunner* New_ProcessRunner(const char* binary, char** args, int count, bool verbose) {
    ProcessRunner* runner = (ProcessRunner*)malloc(sizeof(ProcessRunner));
    runner->binary_path = binary;
    runner->args = args;
    runner->arg_count = count;
    runner->verbose = verbose;

    runner->BuildCommandLine = ProcessRunner_BuildCommandLine;
    runner->Run              = ProcessRunner_Run;
    return runner;
}

void Delete_ProcessRunner(ProcessRunner* this) {
    if (this) free(this);
}

/* ============================================================================
 * CLASS: CcApp
 * Master controller object that links configuration, discovery, and execution.
 * ============================================================================ */
typedef struct CcApp CcApp;

struct CcApp {
    CcConfig*        config;
    CompilerLocator* locator;

    int (*Execute)(CcApp* this);
};

static int CcApp_Execute(CcApp* this) {
    char* targetBinary = NULL;

    if (this->config->explicit_compiler_path) {
        targetBinary = _strdup(this->config->explicit_compiler_path);
    } else {
        targetBinary = this->locator->Locate(this->locator, this->config->preference);
    }

    if (!targetBinary) {
        fprintf(stderr, "cc: fatal error: unable to locate Clang or GCC on this system.\n");
        fprintf(stderr, "Try running 'cc --cc-help' or supply '--cc-use-path=<path>'.\n");
        return EXIT_FAILURE;
    }

    /* Handle --cc-show-path */
    if (this->config->show_path_only) {
        printf("%s\n", targetBinary);
        free(targetBinary);
        return EXIT_SUCCESS;
    }

    if (this->config->verbose) {
        printf("[cc:info] Resolved Backend: %s\n", targetBinary);
    }

    /* Handle --cc-dry-run */
    if (this->config->dry_run) {
        ProcessRunner* runner = New_ProcessRunner(targetBinary, this->config->compiler_args, this->config->arg_count, false);
        char* cmdLine = runner->BuildCommandLine(runner);
        printf("%s\n", cmdLine);
        free(cmdLine);
        Delete_ProcessRunner(runner);
        free(targetBinary);
        return EXIT_SUCCESS;
    }

    /* Dispatch to compiler */
    ProcessRunner* runner = New_ProcessRunner(
        targetBinary,
        this->config->compiler_args,
        this->config->arg_count,
        this->config->verbose
    );

    int exitCode = runner->Run(runner);

    Delete_ProcessRunner(runner);
    free(targetBinary);

    return exitCode;
}

CcApp* New_CcApp(CcConfig* config) {
    CcApp* app = (CcApp*)malloc(sizeof(CcApp));
    app->config = config;
    app->locator = New_CompilerLocator(config->verbose);
    app->Execute = CcApp_Execute;
    return app;
}

void Delete_CcApp(CcApp* this) {
    if (!this) return;
    Delete_CompilerLocator(this->locator);
    free(this);
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */
int main(int argc, char** argv) {
    CcConfig* config = New_CcConfig();
    if (!config->Parse(config, argc, argv)) {
        Delete_CcConfig(config);
        return EXIT_FAILURE;
    }

    CcApp* app = New_CcApp(config);
    int exitCode = app->Execute(app);

    Delete_CcApp(app);
    Delete_CcConfig(config);

    return exitCode;
}