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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

// ============================================================================
// 1. DATA MODELS & PASS CONFIGURATION
// ============================================================================

enum PassType { PASS_ZERO, PASS_ONE, PASS_PATTERN, PASS_RANDOM };

struct PassConfig {
    PassType type;
    unsigned char pattern = 0x00;
};

struct WipeOptions {
    bool force = false;
    bool recursive = false;
    bool verbose = false;
    bool interactive = false;
    bool quick_mode = false;
    bool dod_mode = false;
    int passes = 4;
    std::vector<std::string> targets;
};

// ============================================================================
// 2. PATH UTILITIES & RANDOM ENGINE
// ============================================================================

class PathUtils {
private:
    static std::mt19937_64& GetRng() {
        static std::mt19937_64 rng(std::random_device{}());
        return rng;
    }

public:
    static std::string RandomString(size_t length) {
        static const char charset[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
        std::string str;
        str.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            str += charset[dist(GetRng())];
        }
        return str;
    }

    static std::string NormalizePath(const std::string& path) {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        while (normalized.size() > 1 && normalized.back() == '\\') {
            normalized.pop_back();
        }
        if (normalized.size() == 2 && normalized[1] == ':') {
            normalized.push_back('\\');
        }
        return normalized;
    }

    static std::string JoinPath(const std::string& base, const std::string& child) {
        std::string result = NormalizePath(base);
        if (result.empty()) {
            return child;
        }
        if (result.back() == '\\') {
            return result + child;
        }
        return result + "\\" + child;
    }

    static std::string GetParentDir(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) return "";
        return path.substr(0, pos + 1);
    }

    static std::string GetRandomFilePath(const std::string& original_path) {
        std::string parent = GetParentDir(original_path);
        return parent + RandomString(12) + ".tmp";
    }

    static std::mt19937_64& Rng() {
        return GetRng();
    }
};

// ============================================================================
// 3. PASS GENERATOR & BUFFER ENGINE
// ============================================================================

class PassGenerator {
public:
    static std::vector<PassConfig> GetPassConfigs(int passes, bool dod_mode, bool quick_mode) {
        std::vector<PassConfig> configs;
        if (quick_mode) {
            configs.push_back({PASS_RANDOM, 0});
            configs.push_back({PASS_ZERO, 0x00});
            return configs;
        }
        if (dod_mode) {
            configs.push_back({PASS_ZERO, 0x00});
            configs.push_back({PASS_ONE, 0xFF});
            configs.push_back({PASS_RANDOM, 0});
            return configs;
        }

        if (passes <= 1) {
            configs.push_back({PASS_RANDOM, 0});
        } else if (passes == 2) {
            configs.push_back({PASS_RANDOM, 0});
            configs.push_back({PASS_ZERO, 0x00});
        } else {
            configs.push_back({PASS_ZERO, 0x00});
            configs.push_back({PASS_ONE, 0xFF});
            for (int i = 2; i < passes - 1; ++i) {
                if (i % 2 == 0) configs.push_back({PASS_PATTERN, 0xAA});
                else configs.push_back({PASS_PATTERN, 0x55});
            }
            configs.push_back({PASS_RANDOM, 0});
        }
        return configs;
    }

    static void FillBuffer(std::vector<char>& buf, const PassConfig& cfg) {
        if (cfg.type == PASS_ZERO) {
            std::fill(buf.begin(), buf.end(), 0x00);
        } else if (cfg.type == PASS_ONE) {
            std::fill(buf.begin(), buf.end(), static_cast<char>(0xFF));
        } else if (cfg.type == PASS_PATTERN) {
            std::fill(buf.begin(), buf.end(), static_cast<char>(cfg.pattern));
        } else if (cfg.type == PASS_RANDOM) {
            std::uniform_int_distribution<unsigned int> dist(0, 255);
            for (size_t i = 0; i < buf.size(); ++i) {
                buf[i] = static_cast<char>(dist(PathUtils::Rng()));
            }
        }
    }
};

// ============================================================================
// 4. SECURE WIPER & DIRECTORY TRAVERSER
// ============================================================================

class FileWiper {
public:
    static bool SecureRenameAndDelete(const std::string& filepath, bool verbose) {
        std::string current_path = filepath;
        
        for (int i = 0; i < 3; ++i) {
            std::string new_path = PathUtils::GetRandomFilePath(current_path);
            if (MoveFileA(current_path.c_str(), new_path.c_str())) {
                current_path = new_path;
            } else {
                break;
            }
        }

        if (DeleteFileA(current_path.c_str())) {
            if (verbose) {
                std::cout << "[+] Wiped & Deleted: " << filepath << "\n";
            }
            return true;
        } else {
            std::cerr << "[-] Failed to delete: " << current_path << " (Error " << GetLastError() << ")\n";
            return false;
        }
    }

    static bool WipeFile(const std::string& filepath, const std::vector<PassConfig>& passes, bool force, bool verbose) {
        if (force) {
            SetFileAttributesA(filepath.c_str(), FILE_ATTRIBUTE_NORMAL);
        }

        HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE | GENERIC_READ,
                                   FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                   FILE_FLAG_WRITE_THROUGH, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Cannot open file: " << filepath << " (Error " << GetLastError() << ")\n";
            return false;
        }

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) {
            std::cerr << "[-] Cannot query size for: " << filepath << "\n";
            CloseHandle(hFile);
            return false;
        }

        uint64_t total_bytes = static_cast<uint64_t>(file_size.QuadPart);
        const size_t BUFFER_SIZE = 65536;
        std::vector<char> buffer(BUFFER_SIZE);

        for (size_t p = 0; p < passes.size(); ++p) {
            if (verbose) {
                std::cout << "[*] Wiping " << filepath << " - Pass " << (p + 1) << "/" << passes.size() << "...\n";
            }

            LARGE_INTEGER zero = {0};
            SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);

            uint64_t bytes_written_total = 0;
            PassGenerator::FillBuffer(buffer, passes[p]);

            while (bytes_written_total < total_bytes) {
                DWORD to_write = static_cast<DWORD>((std::min)(static_cast<uint64_t>(BUFFER_SIZE), total_bytes - bytes_written_total));
                
                if (passes[p].type == PASS_RANDOM) {
                    PassGenerator::FillBuffer(buffer, passes[p]);
                }

                DWORD written = 0;
                if (!WriteFile(hFile, buffer.data(), to_write, &written, NULL) || written == 0) {
                    std::cerr << "[-] Write error on file: " << filepath << " (Error " << GetLastError() << ")\n";
                    CloseHandle(hFile);
                    return false;
                }
                bytes_written_total += written;
            }

            FlushFileBuffers(hFile);
        }

        LARGE_INTEGER zero = {0};
        SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
        CloseHandle(hFile);

        return SecureRenameAndDelete(filepath, verbose);
    }
};

class DirectoryTraverser {
public:
    static bool WipeDirectory(const std::string& dirpath, const std::vector<PassConfig>& passes,
                              bool recursive, bool force, bool verbose, bool interactive) {
        std::string search_path = PathUtils::JoinPath(dirpath, "*");
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);

        if (hFind == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Cannot access directory: " << dirpath << "\n";
            return false;
        }

        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;

            std::string full_path = PathUtils::JoinPath(dirpath, name);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    if (force) SetFileAttributesA(full_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                    RemoveDirectoryA(full_path.c_str());
                } else if (recursive) {
                    WipeDirectory(full_path, passes, recursive, force, verbose, interactive);
                }
            } else {
                if (interactive) {
                    std::cout << "Wipe file '" << full_path << "'? (y/N): ";
                    char ans = 'n';
                    std::cin >> ans;
                    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                    if (ans != 'y' && ans != 'Y') continue;
                }
                FileWiper::WipeFile(full_path, passes, force, verbose);
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);

        if (force) SetFileAttributesA(dirpath.c_str(), FILE_ATTRIBUTE_NORMAL);

        std::string current_dir = dirpath;
        for (int i = 0; i < 3; ++i) {
            std::string new_dir = PathUtils::GetRandomFilePath(current_dir);
            if (MoveFileA(current_dir.c_str(), new_dir.c_str())) {
                current_dir = new_dir;
            } else {
                break;
            }
        }

        if (RemoveDirectoryA(current_dir.c_str())) {
            if (verbose) {
                std::cout << "[+] Removed directory: " << dirpath << "\n";
            }
            return true;
        } else {
            std::cerr << "[-] Failed to remove directory: " << current_dir << " (Error " << GetLastError() << ")\n";
            return false;
        }
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class UserPrompt {
public:
    static bool ConfirmAction(const std::string& target) {
        std::cout << "Are you sure you want to securely wipe '" << target << "'? (y/N): ";
        std::string resp;
        std::getline(std::cin, resp);
        if (resp.empty()) return false;
        std::transform(resp.begin(), resp.end(), resp.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return resp == "y" || resp == "yes";
    }
};

class OptionParser {
public:
    static void PrintUsage(const char* prog_name) {
        (void)prog_name;
        std::cout << R"(wipe(1)                   CrossShell for UNIX Reference Manual                 wipe(1)

    NAME
        wipe - securely erase files and directory trees by overwriting data

    SYNOPSIS
        wipe [OPTIONS] [FILE/DIR...]

    DESCRIPTION
        Securely erase files and directory trees by overwriting data.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -f, --force
            Force deletion without prompt and override read-only.

        -r, -R, --recursive
            Remove directories and their contents recursively.

        -v, --verbose
            Verbose mode, output detailed wipe progress.

        -i, --interactive
            Prompt before wiping each file.

        -q, --quick
            Quick mode (2 passes: random + zeroes).

        -d, --dod
            DoD 5220.22-M mode (3 passes: zeroes, ones, random).

        -p, --passes N
            Set custom number of overwrite passes (default: 4).

        -h, --help, /?, -?
            Display this help and exit.

        --version
            Output version information and exit.

    EXAMPLES
        wipe file.txt
            Securely wipe a single file.

        wipe -r -f secret_dir
            Recursively wipe directory without confirmation prompts.

        wipe -q large_data.bin
            Quick wipe using 2 overwrite passes.

    CrossShell for UNIX                                                    wipe(1)
)";
    }

    bool Parse(int argc, char* argv[], WipeOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                PrintUsage(argv[0]);
                exitEarly = true;
                return true;
            } else if (arg == "--version") {
                std::cout << "wipe (CrossShell) 1.0\n";
                exitEarly = true;
                return true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
            } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
                opts.recursive = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-i" || arg == "--interactive") {
                opts.interactive = true;
            } else if (arg == "-q" || arg == "--quick") {
                opts.quick_mode = true;
            } else if (arg == "-d" || arg == "--dod") {
                opts.dod_mode = true;
            } else if (arg == "-p" || arg == "--passes") {
                if (i + 1 < argc) {
                    char* end = nullptr;
                    long parsed = std::strtol(argv[++i], &end, 10);
                    if (end == nullptr || *end != '\0' || parsed < 1 || parsed > 1000) {
                        std::cerr << "wipe: invalid number of passes: '" << argv[i] << "'\n";
                        return false;
                    }
                    opts.passes = static_cast<int>(parsed);
                } else {
                    std::cerr << "wipe: option '-p' requires an argument\n";
                    return false;
                }
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "wipe: invalid option '" << arg << "'\n";
                return false;
            } else {
                opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            std::cerr << "wipe: missing file or directory operand\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return false;
        }

        return true;
    }
};

class WipeApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        WipeOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::vector<PassConfig> pass_configs = PassGenerator::GetPassConfigs(opts.passes, opts.dod_mode, opts.quick_mode);

        for (auto& target : opts.targets) {
            target = PathUtils::NormalizePath(target);

            DWORD attrs = GetFileAttributesA(target.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::cerr << "wipe: cannot access '" << target << "': No such file or directory\n";
                continue;
            }

            if (!opts.force && !opts.interactive) {
                if (!UserPrompt::ConfirmAction(target)) {
                    std::cout << "Skipped '" << target << "'.\n";
                    continue;
                }
            }

            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                if (!opts.recursive) {
                    std::cerr << "wipe: '" << target << "' is a directory (use -r to recurse)\n";
                    continue;
                }
                DirectoryTraverser::WipeDirectory(target, pass_configs, opts.recursive, opts.force, opts.verbose, opts.interactive);
            } else {
                FileWiper::WipeFile(target, pass_configs, opts.force, opts.verbose);
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    WipeApplication app;
    return app.Run(argc, argv);
}
