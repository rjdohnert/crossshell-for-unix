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

