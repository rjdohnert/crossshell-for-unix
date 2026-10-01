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

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cctype>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

// ==========================================
// Bitwise Packing Helper Classes
// ==========================================

enum class ArchiveFormat {
    Z,
    Zip,
    TarGz
};

bool path_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool is_directory(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path), ec);
}

std::string file_extension(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    if (slash != std::string::npos && dot < slash) {
        return "";
    }
    return path.substr(dot);
}

std::string strip_extension(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return path;
    }
    if (slash != std::string::npos && dot < slash) {
        return path;
    }
    return path.substr(0, dot);
}

std::string file_name_only(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string parent_path(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return path.substr(0, 1);
    }
    if (slash == 2 && path.size() >= 3 && path[1] == ':') {
        return path.substr(0, 3);
    }
    return path.substr(0, slash);
}

std::string to_lower(std::string value) {
    for (size_t i = 0; i < value.size(); ++i) {
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return value;
}

bool ends_with_icase(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    std::string v = to_lower(value.substr(value.size() - suffix.size()));
    std::string s = to_lower(suffix);
    return v == s;
}

std::string strip_known_archive_extension(const std::string& path) {
    if (ends_with_icase(path, ".tar.gz")) {
        return path.substr(0, path.size() - 7);
    }
    if (ends_with_icase(path, ".zip")) {
        return path.substr(0, path.size() - 4);
    }
    return strip_extension(path);
}

std::string shell_quote(const std::string& input) {
    std::string result = "\"";
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '"') {
            result += "\\\"";
        } else {
            result += input[i];
        }
    }
    result += "\"";
    return result;
}

std::string normalize_path(const std::string& path) {
    std::error_code ec;
    auto absolute_path = std::filesystem::absolute(std::filesystem::path(path), ec);
    if (ec) {
        return std::filesystem::path(path).lexically_normal().string();
    }

    auto canonical_path = std::filesystem::weakly_canonical(absolute_path, ec);
    if (ec) {
        return absolute_path.lexically_normal().string();
    }
    return canonical_path.string();
}

bool is_same_path(const std::string& left, const std::string& right) {
    return normalize_path(left) == normalize_path(right);
}

std::string output_path_for_compress(const std::string& filepath, ArchiveFormat format) {
    std::string output_path = filepath;

    if (format == ArchiveFormat::Zip) {
        if (ends_with_icase(output_path, ".zip")) {
            return output_path;
        }
        return output_path + ".zip";
    }

    if (format == ArchiveFormat::TarGz) {
        if (ends_with_icase(output_path, ".tar.gz")) {
            return output_path;
        }
        return output_path + ".tar.gz";
    }

    if (ends_with_icase(output_path, ".Z") || ends_with_icase(output_path, ".z")) {
        return output_path;
    }
    return output_path + ".Z";
}

std::string output_path_for_decompress(const std::string& filepath) {
    std::string output_path = filepath;
    if (ends_with_icase(output_path, ".tar.gz")) {
        return strip_known_archive_extension(output_path);
    }
    if (ends_with_icase(output_path, ".zip")) {
        return strip_known_archive_extension(output_path);
    }
    if (ends_with_icase(output_path, ".Z") || ends_with_icase(output_path, ".z")) {
        return strip_extension(output_path);
    }
    return output_path + ".uncompressed";
}

std::string current_directory() {
    char* cwd = nullptr;
#ifdef _WIN32
    cwd = _getcwd(nullptr, 0);
#else
    cwd = getcwd(nullptr, 0);
#endif
    if (cwd == nullptr) {
        return "";
    }
    std::string result(cwd);
    free(cwd);
    return result;
}

bool change_directory(const std::string& path) {
#ifdef _WIN32
    return _chdir(path.c_str()) == 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

bool confirm_overwrite(const std::string& path) {
    std::cerr << path << " already exists; overwrite (y or n)? ";
    char answer;
    std::cin >> answer;
    return answer == 'y' || answer == 'Y';
}

void print_help(const std::string& exe_name = "compress") {
    (void)exe_name;
    std::cout << R"(compress(1)             CrossShell for UNIX Reference Manual           compress(1)

    NAME
        compress - compress or decompress files using LZW and archive formats

    SYNOPSIS
        compress [OPTIONS] [FILE_OR_FOLDER...]

    DESCRIPTION
        Compresses files using the historical UNIX LZW compression format (.Z),
        or creates Zip (.zip) and Tar/Gzip (.tar.gz) archives. If no file operand
        is specified, compress reads standard input and writes to standard output.

    OPTIONS
        -d, --decompress
            Decompress input (.Z), or extract .zip/.tar.gz archives.

        -f, --force
            Overwrite existing output files without prompting.

        -c, --stdout
            Write output to stdout (.Z mode only).

        -v, --verbose
            Display detailed execution diagnostics.

        -k, --keep
            Keep original input files after successful operation.

        -z, --zip
            Create .zip archives (supports files and folders).

        -t, --tar-gz
            Create .tar.gz archives (supports files and folders).

        --json, --csv, --table
            Emit operation status records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send operation status directly through COMMAND.

        -h, --help
            Display this reference manual.

    EXAMPLES
        compress file.txt
            Compress file.txt to file.txt.Z.

        compress -d archive.Z
            Decompress archive.Z to original file.

        compress --zip myfolder
            Create Zip archive of myfolder.

        compress --tar-gz myfolder
            Create tar.gz archive of myfolder.

    CrossShell for UNIX                                                    compress(1)
)";
}

bool create_tar_archive(const std::string& input_path, ArchiveFormat format, const std::string& output_path, bool verbose) {
    std::string flags = (format == ArchiveFormat::Zip) ? "-a -cf" : "-czf";
    std::string parent = parent_path(input_path);
    std::string name = file_name_only(input_path);

    std::string cmd = "tar " + flags + " " + shell_quote(output_path) + " " + shell_quote(name);

    std::string original_dir = current_directory();
    if (!change_directory(parent)) {
        std::cerr << input_path << ": Failed to switch to parent directory for archiving\n";
        return false;
    }

    int rc = std::system(cmd.c_str());
    if (!original_dir.empty()) {
        change_directory(original_dir);
    }

    if (rc != 0) {
        std::cerr << input_path << ": tar command failed\n";
        return false;
    }

    if (verbose) {
        std::cerr << input_path << " -- archived to " << output_path << "\n";
    }
    return true;
}

bool extract_archive(const std::string& archive_path, bool verbose) {
    std::string dest = parent_path(archive_path);
    std::string cmd = "tar -xf " + shell_quote(archive_path) + " -C " + shell_quote(dest);

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::cerr << archive_path << ": extraction failed\n";
        return false;
    }

    if (verbose) {
        std::cerr << archive_path << " -- extracted to " << dest << "\n";
    }
    return true;
}

class BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
public:
    BitWriter(std::vector<uint8_t>& output) : out(output) {}
    
    void write(uint32_t code, int num_bits) {
        buffer |= (code << bits_in_buffer);
        bits_in_buffer += num_bits;
        while (bits_in_buffer >= 8) {
            out.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer >>= 8;
            bits_in_buffer -= 8;
        }
    }
    
    void flush() {
        if (bits_in_buffer > 0) {
            out.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer = 0;
            bits_in_buffer = 0;
        }
    }
};

class BitReader {
    const std::vector<uint8_t>& in;
    size_t byte_idx = 3; // LZW data starts after the 3-byte header
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
public:
    BitReader(const std::vector<uint8_t>& input) : in(input) {}
    
    bool read(uint32_t& code, int num_bits) {
        while (bits_in_buffer < num_bits) {
            if (byte_idx >= in.size()) {
                if (bits_in_buffer == 0) return false;
                break; 
            }
            buffer |= (static_cast<uint32_t>(in[byte_idx]) << bits_in_buffer);
            byte_idx++;
            bits_in_buffer += 8;
        }
        if (bits_in_buffer < num_bits) {
            return false;
        }
        code = buffer & ((1ULL << num_bits) - 1);
        buffer >>= num_bits;
        bits_in_buffer -= num_bits;
        return true;
    }
};

// ==========================================
// LZW Core Logic
// ==========================================

struct DecodeNode {
    uint16_t parent = 0;
    uint8_t character = 0;
};

void get_string(uint32_t code, const std::vector<DecodeNode>& dict, std::vector<uint8_t>& out_str) {
    if (code < 256) {
        out_str.push_back(static_cast<uint8_t>(code));
        return;
    }
    size_t start_idx = out_str.size();
    uint32_t curr = code;
    size_t depth = 0;
    while (curr >= 256) {
        out_str.push_back(dict[curr].character);
        curr = dict[curr].parent;
        depth++;
        if (depth > 65536) {
            throw std::runtime_error("Corrupted file: LZW loop detected");
        }
    }
    out_str.push_back(static_cast<uint8_t>(curr));
    std::reverse(out_str.begin() + start_idx, out_str.end());
}

std::vector<uint8_t> compress_lzw(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    if (input.empty()) return output;

    // Unix .Z Header: 0x1F, 0x9D, 0x90 (16 max bits, block mode)
    output.push_back(0x1F);
    output.push_back(0x9D);
    output.push_back(0x90);

    BitWriter bit_writer(output);

    struct LZWNode {
        uint16_t children[256] = {0};
    };
    std::vector<LZWNode> trie(65536);

    uint16_t next_code = 257;
    int current_bit_width = 9;
    const int max_bits = 16;
    const uint32_t max_code = (1ULL << max_bits);

    uint16_t p = input[0];

    for (size_t i = 1; i < input.size(); ++i) {
        uint8_t c = input[i];
        uint16_t child = trie[p].children[c];
        if (child != 0) {
            p = child;
        } else {
            bit_writer.write(p, current_bit_width);

            if (next_code < max_code) {
                trie[p].children[c] = next_code;
                next_code++;
                if (next_code == (1ULL << current_bit_width) + 1 && current_bit_width < max_bits) {
                    current_bit_width++;
                }
            } else {
                // Dictionary full: Output clear code (256) and reset dictionary
                bit_writer.write(256, current_bit_width);
                for (int j = 0; j < 65536; ++j) {
                    std::memset(trie[j].children, 0, sizeof(trie[j].children));
                }
                next_code = 257;
                current_bit_width = 9;
            }
            p = c;
        }
    }
    bit_writer.write(p, current_bit_width);
    bit_writer.flush();

    return output;
}

std::vector<uint8_t> decompress_lzw(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    if (input.size() < 3) return output;

    if (input[0] != 0x1F || input[1] != 0x9D) {
        throw std::runtime_error("Invalid file format (missing magic header)");
    }

    int max_bits = input[2] & 0x1F;
    bool block_mode = (input[2] & 0x80) != 0;

    if (max_bits < 9 || max_bits > 16) {
        throw std::runtime_error("Unsupported max bits in header");
    }

    BitReader bit_reader(input);
    std::vector<DecodeNode> dict(65536);

    uint16_t next_code = 257;
    int current_bit_width = 9;
    const uint32_t max_code = (1ULL << max_bits);

    uint32_t old_code;
    if (!bit_reader.read(old_code, current_bit_width)) {
        return output; 
    }

    get_string(old_code, dict, output);

    uint32_t new_code;
    while (bit_reader.read(new_code, current_bit_width)) {
        if (new_code == 256 && block_mode) {
            next_code = 257;
            current_bit_width = 9;
            if (!bit_reader.read(old_code, current_bit_width)) {
                break;
            }
            get_string(old_code, dict, output);
            continue;
        }

        std::vector<uint8_t> s;
        if (new_code < next_code) {
            get_string(new_code, dict, s);
        } else if (new_code == next_code) {
            get_string(old_code, dict, s);
            s.push_back(s[0]);
        } else {
            throw std::runtime_error("Corrupted compressed stream: invalid code");
        }

        output.insert(output.end(), s.begin(), s.end());

        if (next_code < max_code) {
            dict[next_code].parent = old_code;
            dict[next_code].character = s[0];
            next_code++;
            if (next_code == (1ULL << current_bit_width) + 1 && current_bit_width < max_bits) {
                current_bit_width++;
            }
        }
        old_code = new_code;
    }

    return output;
}

// ==========================================
// Streaming and File I/O Helpers
// ==========================================

std::vector<uint8_t> read_all_stdin() {
    std::vector<uint8_t> data;
    char buf[16384];
    while (std::cin.read(buf, sizeof(buf))) {
        data.insert(data.end(), buf, buf + std::cin.gcount());
    }
    data.insert(data.end(), buf, buf + std::cin.gcount());
    return data;
}

void write_all_stdout(const std::vector<uint8_t>& data) {
    std::cout.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void process_file(const std::string& filepath, bool decompress, bool force, bool to_stdout, bool verbose, bool keep, ArchiveFormat format) {
    if (!path_exists(filepath)) {
        std::cerr << filepath << ": No such file or directory\n";
        return;
    }

    bool is_archive = ends_with_icase(filepath, ".zip") || ends_with_icase(filepath, ".tar.gz");

    if (decompress && is_archive) {
        if (to_stdout) {
            std::cerr << filepath << ": --stdout is not supported for archive extraction\n";
            return;
        }

        if (!extract_archive(filepath, verbose)) {
            return;
        }

        if (!keep) {
            std::remove(filepath.c_str());
        }
        return;
    }

    if (!decompress && (format == ArchiveFormat::Zip || format == ArchiveFormat::TarGz)) {
        if (to_stdout) {
            std::cerr << filepath << ": --stdout is not supported for archive creation\n";
            return;
        }

        std::string out_p = output_path_for_compress(filepath, format);
        if (is_same_path(filepath, out_p)) {
            std::cerr << filepath << ": output path would overwrite the input file\n";
            return;
        }

        if (path_exists(out_p) && !force) {
            if (!confirm_overwrite(out_p)) {
                std::cerr << "File not overwritten\n";
                return;
            }
        }

        if (!create_tar_archive(filepath, format, out_p, verbose)) {
            return;
        }

        if (!keep && !is_directory(filepath)) {
            std::remove(filepath.c_str());
        }
        return;
    }

    if (is_directory(filepath)) {
        std::cerr << filepath << ": directory input requires --zip or --tar-gz\n";
        return;
    }

    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        std::cerr << filepath << ": Failed to open file\n";
        return;
    }

    std::vector<uint8_t> in_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (decompress) {
        try {
            std::vector<uint8_t> out_data = decompress_lzw(in_data);
            if (to_stdout) {
                write_all_stdout(out_data);
            } else {
                std::string out_p = output_path_for_decompress(filepath);
                if (is_same_path(filepath, out_p)) {
                    std::cerr << filepath << ": output path would overwrite the input file\n";
                    return;
                }

                if (path_exists(out_p) && !force) {
                    if (!confirm_overwrite(out_p)) {
                        std::cerr << "File not overwritten\n";
                        return;
                    }
                }

                std::ofstream out(out_p, std::ios::binary);
                if (!out) {
                    std::cerr << out_p << ": Failed to write output file\n";
                    return;
                }
                out.write(reinterpret_cast<const char*>(out_data.data()), out_data.size());
                out.close();

                if (!keep) {
                    std::remove(filepath.c_str());
                }
                if (verbose) {
                    std::cerr << filepath << " -- decompressed to " << out_p << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << filepath << ": " << e.what() << "\n";
        }
    } else {
        try {
            std::vector<uint8_t> out_data = compress_lzw(in_data);
            
            bool reduced = out_data.size() < in_data.size();
            if (!reduced && !force && !to_stdout) {
                if (verbose) {
                    double ratio = 100.0 * (static_cast<double>(in_data.size()) - out_data.size()) / in_data.size();
                    std::cerr << filepath << ": compression: " << std::fixed << std::setprecision(2) << ratio << "% -- file unchanged\n";
                } else {
                    std::cerr << filepath << ": file unchanged\n";
                }
                return;
            }

            if (to_stdout) {
                write_all_stdout(out_data);
            } else {
                std::string out_p = output_path_for_compress(filepath, ArchiveFormat::Z);
                if (is_same_path(filepath, out_p)) {
                    std::cerr << filepath << ": output path would overwrite the input file\n";
                    return;
                }

                if (path_exists(out_p) && !force) {
                    if (!confirm_overwrite(out_p)) {
                        std::cerr << "File not overwritten\n";
                        return;
                    }
                }

                std::ofstream out(out_p, std::ios::binary);
                if (!out) {
                    std::cerr << out_p << ": Failed to write output file\n";
                    return;
                }
                out.write(reinterpret_cast<const char*>(out_data.data()), out_data.size());
                out.close();

                if (!keep) {
                    std::remove(filepath.c_str());
                }

                if (verbose) {
                    double ratio = 100.0 * (static_cast<double>(in_data.size()) - out_data.size()) / in_data.size();
                    std::cerr << filepath << ": -- replaced with " << out_p 
                              << " (" << std::fixed << std::setprecision(2) << ratio << "% compression)\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << filepath << ": " << e.what() << "\n";
        }
    }
}

// ==========================================
// Entry Point
// ==========================================

static int compress_main(int argc, char* argv[]) {
    bool decompress_mode = false;
    bool force_mode = false;
    bool stdout_mode = false;
    bool verbose_mode = false;
    bool keep_mode = false;
    bool help_mode = false;
    ArchiveFormat format = ArchiveFormat::Z;
    std::vector<std::string> files;

    // Unix mimicry: check if executable name contains "uncompress"
    std::string exe_name = file_name_only(argv[0]);
    for (char& c : exe_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (exe_name.find("uncompress") != std::string::npos) {
        decompress_mode = true;
    }

    // Parse options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.size() > 1) {
            if (arg[1] == '-') {
                if (arg == "--decompress") decompress_mode = true;
                else if (arg == "--force") force_mode = true;
                else if (arg == "--stdout") stdout_mode = true;
                else if (arg == "--verbose") verbose_mode = true;
                else if (arg == "--keep") keep_mode = true;
                else if (arg == "--zip") format = ArchiveFormat::Zip;
                else if (arg == "--tar-gz") format = ArchiveFormat::TarGz;
                else if (arg == "--help") help_mode = true;
                else if (arg == "--json" || arg == "--csv" || arg == "--table" || arg == "--pipe") { }
                else {
                    std::cerr << "Unknown option: " << arg << "\n";
                    return 1;
                }
            } else {
                for (size_t j = 1; j < arg.size(); ++j) {
                    char opt = arg[j];
                    if (opt == 'd') decompress_mode = true;
                    else if (opt == 'f') force_mode = true;
                    else if (opt == 'c') stdout_mode = true;
                    else if (opt == 'v') verbose_mode = true;
                    else if (opt == 'k') keep_mode = true;
                    else if (opt == 'z') format = ArchiveFormat::Zip;
                    else if (opt == 't') format = ArchiveFormat::TarGz;
                    else if (opt == 'h') help_mode = true;
                    else {
                        std::cerr << "Unknown option: -" << opt << "\n";
                        return 1;
                    }
                }
            }
        } else {
            files.push_back(arg);
        }
    }

#ifdef _WIN32
    // Setup binary modes for standard input/output redirection on Windows
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (help_mode) {
        print_help(exe_name);
        return 0;
    }

    if (files.empty()) {
        if (format != ArchiveFormat::Z) {
            std::cerr << "Archive format options require one or more file/folder paths\n";
            return 1;
        }

        std::vector<uint8_t> in_data = read_all_stdin();
        if (decompress_mode) {
            try {
                std::vector<uint8_t> out_data = decompress_lzw(in_data);
                write_all_stdout(out_data);
            } catch (const std::exception& e) {
                std::cerr << "Decompression error: " << e.what() << "\n";
                return 1;
            }
        } else {
            try {
                std::vector<uint8_t> out_data = compress_lzw(in_data);
                write_all_stdout(out_data);
            } catch (const std::exception& e) {
                std::cerr << "Compression error: " << e.what() << "\n";
                return 1;
            }
        }
        return 0;
    }

    for (const auto& filepath : files) {
        process_file(filepath, decompress_mode, force_mode, stdout_mode, verbose_mode, keep_mode, format);
    }

    return 0;
}

class CompressApplication { public: int run(int argc, char* argv[]) const { return compress_main(argc, argv); } };
int main(int argc, char* argv[]) { return CompressApplication().run(argc, argv); }
