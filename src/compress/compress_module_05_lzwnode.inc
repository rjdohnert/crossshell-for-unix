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

