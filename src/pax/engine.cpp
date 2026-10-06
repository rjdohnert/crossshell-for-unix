#include "engine.hpp"

// ============================================================================
// OctalCodec Implementation
// ============================================================================

uint64_t OctalCodec::ParseOctal(const char* str, size_t size) {
    uint64_t val = 0;
    for (size_t i = 0; i < size; ++i) {
        if (str[i] < '0' || str[i] > '7') {
            if (str[i] == '\0' || str[i] == ' ') continue;
            break;
        }
        val = (val << 3) + (str[i] - '0');
    }
    return val;
}

void OctalCodec::FormatOctal(char* dest, size_t size, uint64_t val) {
    std::snprintf(dest, size, "%0*llo", static_cast<int>(size - 1), static_cast<unsigned long long>(val));
}

uint32_t OctalCodec::CalculateChecksum(const TarHeader& hdr) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&hdr);
    uint32_t sum = 0;
    for (size_t i = 0; i < 512; ++i) {
        if (i >= 148 && i < 156) {
            sum += ' ';
        } else {
            sum += bytes[i];
        }
    }
    return sum;
}

std::string OctalCodec::GetFullTarPath(const TarHeader& hdr) {
    std::string name(hdr.name, strnlen(hdr.name, 100));
    std::string prefix(hdr.prefix, strnlen(hdr.prefix, 155));
    if (!prefix.empty()) {
        return prefix + "/" + name;
    }
    return name;
}

// ============================================================================
// SubstitutionEngine Implementation
// ============================================================================

bool SubstitutionEngine::ParseSubstitution(const std::string& arg, std::vector<Substitution>& subs) {
    if (arg.length() < 3) {
        std::cerr << "pax: invalid substitution expression '" << arg << "'\n";
        return false;
    }

    char delim = arg[0];
    size_t pos1 = 1;
    size_t pos2 = arg.find(delim, pos1);
    if (pos2 == std::string::npos) {
        std::cerr << "pax: invalid substitution expression '" << arg << "'\n";
        return false;
    }

    size_t pos3 = arg.find(delim, pos2 + 1);
    if (pos3 == std::string::npos) pos3 = arg.length();

    std::string pattern = arg.substr(pos1, pos2 - pos1);
    std::string replacement = arg.substr(pos2 + 1, pos3 - (pos2 + 1));
    std::string flags = (pos3 < arg.length()) ? arg.substr(pos3 + 1) : "";

    try {
        Substitution sub;
        sub.re = std::regex(pattern);
        sub.replacement = replacement;
        sub.global = (flags.find('g') != std::string::npos);
        subs.push_back(sub);
        return true;
    } catch (const std::regex_error& ex) {
        std::cerr << "pax: invalid regex pattern: " << ex.what() << "\n";
        return false;
    }
}

std::string SubstitutionEngine::ApplySubstitutions(std::string path, const std::vector<Substitution>& subs) {
    for (const auto& sub : subs) {
        if (sub.global) {
            path = std::regex_replace(path, sub.re, sub.replacement);
        } else {
            path = std::regex_replace(path, sub.re, sub.replacement, std::regex_constants::format_first_only);
        }
    }
    return path;
}

// ============================================================================
// PaxArchiver Implementation
// ============================================================================

void PaxArchiver::List(std::istream& in, bool verbose, const std::vector<Substitution>& subs) {
    TarHeader hdr;
    while (in.read(reinterpret_cast<char*>(&hdr), 512)) {
        if (hdr.name[0] == '\0') break;

        std::string full_path = OctalCodec::GetFullTarPath(hdr);
        full_path = SubstitutionEngine::ApplySubstitutions(full_path, subs);
        if (full_path.empty()) continue;

        uint64_t file_size = OctalCodec::ParseOctal(hdr.size, sizeof(hdr.size));

        if (verbose) {
            std::cout << (hdr.typeflag == '5' ? 'd' : '-') << " "
                      << std::setw(10) << file_size << " "
                      << full_path << "\n";
        } else {
            std::cout << full_path << "\n";
        }

        uint64_t blocks = (file_size + 511) / 512;
        in.seekg(blocks * 512, std::ios::cur);
    }
}

void PaxArchiver::Read(std::istream& in, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs) {
    TarHeader hdr;
    char buffer[512];

    while (in.read(reinterpret_cast<char*>(&hdr), 512)) {
        if (hdr.name[0] == '\0') break;

        std::string raw_path = OctalCodec::GetFullTarPath(hdr);
        std::string sub_path = SubstitutionEngine::ApplySubstitutions(raw_path, subs);
        if (sub_path.empty()) continue;

        fs::path target_path = dest_dir / sub_path;
        uint64_t file_size = OctalCodec::ParseOctal(hdr.size, sizeof(hdr.size));
        uint64_t blocks = (file_size + 511) / 512;

        if (hdr.typeflag == '5' || sub_path.back() == '/') {
            fs::create_directories(target_path);
            if (verbose) std::cout << target_path.string() << "\n";
            continue;
        }

        if (fs::exists(target_path)) {
            if (keep) {
                in.seekg(blocks * 512, std::ios::cur);
                continue;
            }
            if (update) {
                uint64_t tar_mtime = OctalCodec::ParseOctal(hdr.mtime, sizeof(hdr.mtime));
                auto local_mtime = fs::last_write_time(target_path).time_since_epoch();
                auto local_sec = std::chrono::duration_cast<std::chrono::seconds>(local_mtime).count();
                if (local_sec >= static_cast<int64_t>(tar_mtime)) {
                    in.seekg(blocks * 512, std::ios::cur);
                    continue;
                }
            }
        }

        fs::create_directories(target_path.parent_path());
        std::ofstream out(target_path, std::ios::binary);

        uint64_t bytes_remaining = file_size;
        for (uint64_t b = 0; b < blocks; ++b) {
            in.read(buffer, 512);
            uint64_t to_write = (std::min)(bytes_remaining, static_cast<uint64_t>(512));
            out.write(buffer, to_write);
            bytes_remaining -= to_write;
        }
        out.close();

        if (verbose) std::cout << target_path.string() << "\n";
    }
}

void PaxArchiver::WriteTarEntry(std::ostream& out, const fs::path& path, const std::string& tar_path, bool verbose) {
    TarHeader hdr = {};
    std::string clean_path = tar_path;
    std::replace(clean_path.begin(), clean_path.end(), '\\', '/');

    if (fs::is_directory(path) && clean_path.back() != '/') {
        clean_path += '/';
    }

    if (clean_path.length() > 100) {
        std::strncpy(hdr.name, clean_path.c_str(), 100);
    } else {
        std::strncpy(hdr.name, clean_path.c_str(), sizeof(hdr.name));
    }

    OctalCodec::FormatOctal(hdr.mode, sizeof(hdr.mode), fs::is_directory(path) ? 0755 : 0644);
    OctalCodec::FormatOctal(hdr.uid, sizeof(hdr.uid), 0);
    OctalCodec::FormatOctal(hdr.gid, sizeof(hdr.gid), 0);

    uint64_t file_size = fs::is_regular_file(path) ? fs::file_size(path) : 0;
    OctalCodec::FormatOctal(hdr.size, sizeof(hdr.size), file_size);

    auto ftime = fs::last_write_time(path).time_since_epoch();
    uint64_t mtime_sec = std::chrono::duration_cast<std::chrono::seconds>(ftime).count();
    OctalCodec::FormatOctal(hdr.mtime, sizeof(hdr.mtime), mtime_sec);

    hdr.typeflag = fs::is_directory(path) ? '5' : '0';
    std::strncpy(hdr.magic, "ustar", 6);
    std::strncpy(hdr.version, "00", 2);

    uint32_t chksum = OctalCodec::CalculateChecksum(hdr);
    OctalCodec::FormatOctal(hdr.chksum, sizeof(hdr.chksum), chksum);

    out.write(reinterpret_cast<const char*>(&hdr), 512);

    if (fs::is_regular_file(path) && file_size > 0) {
        std::ifstream in(path, std::ios::binary);
        char buffer[512] = { 0 };
        while (in.read(buffer, 512) || in.gcount() > 0) {
            out.write(buffer, 512);
            std::memset(buffer, 0, 512);
        }
    }

    if (verbose) std::cout << clean_path << "\n";
}

void PaxArchiver::Write(std::ostream& out, const std::vector<fs::path>& sources, bool verbose, const std::vector<Substitution>& subs) {
    for (const auto& src : sources) {
        if (!fs::exists(src)) {
            std::cerr << "pax: " << src.string() << ": No such file or directory\n";
            continue;
        }

        if (fs::is_directory(src)) {
            for (const auto& entry : fs::recursive_directory_iterator(src)) {
                std::string rel_path = entry.path().lexically_normal().string();
                std::string sub_path = SubstitutionEngine::ApplySubstitutions(rel_path, subs);
                if (!sub_path.empty()) {
                    WriteTarEntry(out, entry.path(), sub_path, verbose);
                }
            }
        } else {
            std::string rel_path = src.string();
            std::string sub_path = SubstitutionEngine::ApplySubstitutions(rel_path, subs);
            if (!sub_path.empty()) {
                WriteTarEntry(out, src, sub_path, verbose);
            }
        }
    }

    char zero_block[1024] = { 0 };
    out.write(zero_block, 1024);
}

void PaxArchiver::Copy(const std::vector<fs::path>& sources, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs) {
    fs::create_directories(dest_dir);

    for (const auto& src : sources) {
        if (!fs::exists(src)) {
            std::cerr << "pax: " << src.string() << ": No such file or directory\n";
            continue;
        }

        auto process_item = [&](const fs::path& item_path, const fs::path& rel_base) {
            std::string rel_str = fs::relative(item_path, rel_base).string();
            std::string sub_str = SubstitutionEngine::ApplySubstitutions(rel_str, subs);
            if (sub_str.empty()) return;

            fs::path target_path = dest_dir / sub_str;

            if (fs::is_directory(item_path)) {
                fs::create_directories(target_path);
                if (verbose) std::cout << target_path.string() << "\n";
            } else if (fs::is_regular_file(item_path)) {
                if (fs::exists(target_path)) {
                    if (keep) return;
                    if (update && fs::last_write_time(target_path) >= fs::last_write_time(item_path)) return;
                }
                fs::create_directories(target_path.parent_path());
                fs::copy_file(item_path, target_path, fs::copy_options::overwrite_existing);
                if (verbose) std::cout << target_path.string() << "\n";
            }
        };

        if (fs::is_directory(src)) {
            fs::path parent_base = src.parent_path();
            if (parent_base.empty()) parent_base = ".";
            for (const auto& entry : fs::recursive_directory_iterator(src)) {
                process_item(entry.path(), parent_base);
            }
        } else {
            fs::path target_filename = SubstitutionEngine::ApplySubstitutions(src.filename().string(), subs);
            if (!target_filename.empty()) {
                fs::path target_path = dest_dir / target_filename;
                fs::copy_file(src, target_path, fs::copy_options::overwrite_existing);
                if (verbose) std::cout << target_path.string() << "\n";
            }
        }
    }
}
