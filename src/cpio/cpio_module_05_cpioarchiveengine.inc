class CpioArchiveEngine {
public:
    static void DoCreate(std::ostream& out, bool null_delim, bool verbose) {
        std::string filename;
        uint32_t ino_counter = 1;

        auto read_next_name = [&](std::string& name) -> bool {
            if (null_delim) {
                return static_cast<bool>(std::getline(std::cin, name, '\0'));
            } else {
                return static_cast<bool>(std::getline(std::cin, name));
            }
        };

        auto write_entry = [&](const std::string& path_str) {
            fs::path p(path_str);

            std::string archive_path = p.generic_string();
            if (archive_path.rfind("./", 0) == 0) {
                archive_path = archive_path.substr(2);
            }

            std::error_code ec;
            fs::file_status status = fs::status(p, ec);
            if (ec) {
                std::cerr << "cpio: " << path_str << ": Cannot stat: " << ec.message() << "\n";
                return;
            }

            uint32_t mode = 0644;
            if (fs::is_directory(status)) {
                mode |= 0040000;
                mode |= 0755;
            } else if (fs::is_regular_file(status)) {
                mode |= 0100000;
            } else {
                return;
            }

            uint64_t fsize = fs::is_regular_file(status) ? fs::file_size(p, ec) : 0;
            if (ec) fsize = 0;

            uint32_t mtime_sec = CpioHeaderCodec::GetMtimeSec(p);

            CpioHeader hdr;
            std::memset(&hdr, '0', sizeof(hdr));
            std::memcpy(hdr.magic, "070701", 6);

            CpioHeaderCodec::FormatHex8(hdr.ino, ino_counter++);
            CpioHeaderCodec::FormatHex8(hdr.mode, mode);
            CpioHeaderCodec::FormatHex8(hdr.uid, 0);
            CpioHeaderCodec::FormatHex8(hdr.gid, 0);
            CpioHeaderCodec::FormatHex8(hdr.nlink, 1);
            CpioHeaderCodec::FormatHex8(hdr.mtime, mtime_sec);
            CpioHeaderCodec::FormatHex8(hdr.filesize, static_cast<uint32_t>(fsize));
            CpioHeaderCodec::FormatHex8(hdr.maj, 0);
            CpioHeaderCodec::FormatHex8(hdr.min, 0);
            CpioHeaderCodec::FormatHex8(hdr.rmaj, 0);
            CpioHeaderCodec::FormatHex8(hdr.rmin, 0);
            CpioHeaderCodec::FormatHex8(hdr.namesize, static_cast<uint32_t>(archive_path.length() + 1));
            CpioHeaderCodec::FormatHex8(hdr.chksum, 0);

            out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
            out.write(archive_path.c_str(), archive_path.length() + 1);

            size_t head_bytes = sizeof(hdr) + archive_path.length() + 1;
            size_t head_pad = (4 - (head_bytes % 4)) % 4;
            CpioHeaderCodec::WritePadding(out, head_pad);

            if (fs::is_regular_file(status) && fsize > 0) {
                std::ifstream fin(p, std::ios::binary);
                if (fin) {
                    char buf[65536];
                    uint64_t remaining = fsize;
                    while (remaining > 0 && fin) {
                        size_t to_read = static_cast<size_t>(std::min<uint64_t>(sizeof(buf), remaining));
                        fin.read(buf, to_read);
                        size_t read_bytes = fin.gcount();
                        if (read_bytes == 0) break;
                        out.write(buf, read_bytes);
                        remaining -= read_bytes;
                    }
                }
                size_t file_pad = (4 - (fsize % 4)) % 4;
                CpioHeaderCodec::WritePadding(out, file_pad);
            }

            if (verbose) {
                std::cerr << archive_path << "\n";
            }
        };

        while (read_next_name(filename)) {
            if (filename.empty()) continue;
            write_entry(filename);
        }

        std::string trailer = "TRAILER!!!";
        CpioHeader hdr;
        std::memset(&hdr, '0', sizeof(hdr));
        std::memcpy(hdr.magic, "070701", 6);
        CpioHeaderCodec::FormatHex8(hdr.namesize, static_cast<uint32_t>(trailer.length() + 1));

        out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        out.write(trailer.c_str(), trailer.length() + 1);
        size_t head_bytes = sizeof(hdr) + trailer.length() + 1;
        size_t head_pad = (4 - (head_bytes % 4)) % 4;
        CpioHeaderCodec::WritePadding(out, head_pad);
    }

    static void DoExtractOrList(std::istream& in, bool extract, bool make_dirs, bool verbose) {
        while (in) {
            CpioHeader hdr;
            in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
            if (in.gcount() < static_cast<std::streamsize>(sizeof(hdr))) break;

            if (std::memcmp(hdr.magic, "070701", 6) != 0 && std::memcmp(hdr.magic, "070702", 6) != 0) {
                std::cerr << "cpio: invalid magic number in header\n";
                return;
            }

            uint32_t mode = CpioHeaderCodec::ParseHex8(hdr.mode);
            uint32_t filesize = CpioHeaderCodec::ParseHex8(hdr.filesize);
            uint32_t namesize = CpioHeaderCodec::ParseHex8(hdr.namesize);

            if (namesize == 0) continue;

            std::vector<char> name_buf(namesize);
            in.read(name_buf.data(), namesize);

            std::string filename(name_buf.data(), namesize > 0 ? namesize - 1 : 0);

            size_t head_bytes = sizeof(hdr) + namesize;
            size_t head_pad = (4 - (head_bytes % 4)) % 4;
            CpioHeaderCodec::SkipBytes(in, head_pad);

            if (filename == "TRAILER!!!") break;

            uint32_t type = mode & 0170000;
            bool is_dir = (type == 0040000);

            if (!extract) {
                if (verbose) {
                    std::cout << (is_dir ? 'd' : '-') << " "
                              << std::setw(10) << filesize << " "
                              << filename << "\n";
                } else {
                    std::cout << filename << "\n";
                }

                if (filesize > 0) {
                    CpioHeaderCodec::SkipBytes(in, filesize);
                    size_t file_pad = (4 - (filesize % 4)) % 4;
                    CpioHeaderCodec::SkipBytes(in, file_pad);
                }
            } else {
                if (verbose) {
                    std::cout << filename << "\n";
                }

                fs::path target_path(filename);

                if (is_dir) {
                    if (make_dirs) {
                        fs::create_directories(target_path);
                    }
                    if (filesize > 0) {
                        CpioHeaderCodec::SkipBytes(in, filesize + ((4 - (filesize % 4)) % 4));
                    }
                } else {
                    if (make_dirs && target_path.has_parent_path()) {
                        fs::create_directories(target_path.parent_path());
                    }

                    std::ofstream fout(target_path, std::ios::binary);
                    if (!fout) {
                        std::cerr << "cpio: failed to create " << filename << "\n";
                        CpioHeaderCodec::SkipBytes(in, filesize);
                    } else {
                        char buf[65536];
                        uint32_t remaining = filesize;
                        while (remaining > 0 && in) {
                            size_t to_read = static_cast<size_t>(std::min<uint32_t>(sizeof(buf), remaining));
                            in.read(buf, to_read);
                            size_t read_bytes = in.gcount();
                            if (read_bytes == 0) break;
                            fout.write(buf, read_bytes);
                            remaining -= static_cast<uint32_t>(read_bytes);
                        }
                    }

                    size_t file_pad = (4 - (filesize % 4)) % 4;
                    CpioHeaderCodec::SkipBytes(in, file_pad);
                }
            }
        }
    }

    static void DoPass(const std::string& dest_dir, bool null_delim, bool make_dirs, bool verbose) {
        fs::path dest(dest_dir);

        std::string filename;
        auto read_next_name = [&](std::string& name) -> bool {
            if (null_delim) {
                return static_cast<bool>(std::getline(std::cin, name, '\0'));
            } else {
                return static_cast<bool>(std::getline(std::cin, name));
            }
        };

        while (read_next_name(filename)) {
            if (filename.empty()) continue;

            fs::path src_path(filename);
            fs::path target_path = dest / src_path;

            std::error_code ec;
            fs::file_status status = fs::status(src_path, ec);
            if (ec) {
                std::cerr << "cpio: " << filename << ": " << ec.message() << "\n";
                continue;
            }

            if (fs::is_directory(status)) {
                if (make_dirs) {
                    fs::create_directories(target_path, ec);
                }
            } else if (fs::is_regular_file(status)) {
                if (make_dirs && target_path.has_parent_path()) {
                    fs::create_directories(target_path.parent_path(), ec);
                }
                fs::copy_file(src_path, target_path, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    std::cerr << "cpio: failed to copy " << filename << ": " << ec.message() << "\n";
                }
            }

            if (verbose) {
                std::cout << filename << "\n";
            }
        }
    }
};

// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

