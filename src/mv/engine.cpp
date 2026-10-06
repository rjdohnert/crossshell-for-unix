#include "engine.hpp"

std::string JsonSerializer::escape(const std::string& str) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 32) oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                else oss.put(c);
        }
    }
    return oss.str();
}

std::string ProgressBar::formatSize(uint64_t bytes) {
    constexpr const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    int idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && idx < 4) {
        size /= 1024.0;
        idx++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(idx == 0 ? 0 : 1) << size << " " << suffixes[idx];
    return oss.str();
}

std::string ProgressBar::formatTime(uint64_t seconds) {
    int m = static_cast<int>(seconds / 60);
    int s = static_cast<int>(seconds % 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << m << ":" << std::setw(2) << s;
    return oss.str();
}

void ProgressBar::hideCursor() {
    if (!cursorHidden) {
        std::cout << "\x1b[?25l" << std::flush;
        cursorHidden = true;
    }
}

void ProgressBar::showCursor() {
    if (cursorHidden) {
        std::cout << "\x1b[?25h" << std::flush;
        cursorHidden = false;
    }
}

ProgressBar::ProgressBar() {
    startTime = std::chrono::steady_clock::now();
    lastRenderTime = startTime;
}

ProgressBar::~ProgressBar() {
    showCursor();
}

void ProgressBar::reset(uint64_t fileBytes) {
    totalBytes = fileBytes;
    transferredBytes = 0;
    startTime = std::chrono::steady_clock::now();
    lastRenderTime = startTime;
}

void ProgressBar::update(uint64_t chunkBytes, const std::string& currentFileName, bool forceRender) {
    transferredBytes += chunkBytes;
    auto now = std::chrono::steady_clock::now();
    auto msSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRenderTime).count();

    if (!forceRender && msSinceLast < 75 && transferredBytes < totalBytes) {
        return;
    }
    lastRenderTime = now;

    hideCursor();

    double progress = (totalBytes > 0) ? (static_cast<double>(transferredBytes) / totalBytes) : 1.0;
    progress = (std::min)(1.0, (std::max)(0.0, progress));

    auto elapsedSec = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();
    double speed = (elapsedSec > 0.001) ? (transferredBytes / elapsedSec) : 0.0;
    uint64_t etaSec = (speed > 0.0 && transferredBytes < totalBytes)
                      ? static_cast<uint64_t>((totalBytes - transferredBytes) / speed) : 0;

    int filled = static_cast<int>(progress * barWidth);
    int empty = barWidth - filled;

    std::string displayName = currentFileName;
    if (displayName.length() > 20) {
        displayName = displayName.substr(0, 17) + "...";
    }

    std::ostringstream oss;
    oss << "\r"
        << std::left << std::setw(20) << displayName << " "
        << "[" << std::string(filled, '=')
        << (filled < barWidth ? ">" : "")
        << std::string((empty > 0 ? empty - 1 : 0), ' ') << "] "
        << std::right << std::setw(3) << static_cast<int>(progress * 100.0) << "% "
        << std::setw(9) << formatSize(static_cast<uint64_t>(speed)) << "/s "
        << "ETA " << formatTime(etaSec) << "  \x1b[K";

    std::cout << oss.str() << std::flush;
}

void ProgressBar::finish() {
    showCursor();
    std::cout << "\n" << std::flush;
}

void FileAttributePreserver::preserve(const fs::path& src, const fs::path& dest, bool preserveExtended) {
    std::error_code ec;
    auto lwt = fs::last_write_time(src, ec);
    if (!ec) {
        fs::last_write_time(dest, lwt, ec);
    }

    if (preserveExtended) {
        DWORD attr = GetFileAttributesW(src.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesW(dest.c_str(), attr & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY));
        }
    }
}

MoveEngine::MoveEngine(const MoveOptions& opts) : options(opts) {}

bool MoveEngine::promptOverwrite(const fs::path& dest) {
    std::cout << "mv: overwrite \"" << dest.string() << "\"? (y/n [n]) ";
    std::string response;
    if (!std::getline(std::cin, response)) {
        return false;
    }
    return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
}

bool MoveEngine::prepareDestination(const fs::path& dest, bool& replaceTarget) {
    std::error_code ec;
    replaceTarget = options.force || !options.interactive;

    if (fs::exists(dest, ec)) {
        if (options.interactive) {
            if (!promptOverwrite(dest)) {
                return false;
            }
            replaceTarget = true;
        }
        if (replaceTarget) {
            DWORD attr = GetFileAttributesW(dest.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(dest.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
            }
        }
    }
    return true;
}

bool MoveEngine::copyAndUnlinkFile(const fs::path& src, const fs::path& dest) {
    std::error_code ec;
    uint64_t fileSize = fs::file_size(src, ec);
    if (ec) fileSize = 0;

    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "mv: cannot open \"" << src.string() << "\": Permission denied\n";
        return false;
    }

    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "mv: cannot create target \"" << dest.string() << "\"\n";
        return false;
    }

    if (options.showProgress) {
        progressBar.reset(fileSize);
    }

    constexpr size_t BUFFER_SIZE = 64 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);

    while (in) {
        in.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = in.gcount();
        if (bytesRead > 0) {
            out.write(buffer.data(), bytesRead);
            if (options.showProgress) {
                progressBar.update(static_cast<uint64_t>(bytesRead), src.filename().string());
            }
        }
    }

    if (options.showProgress) {
        progressBar.update(0, src.filename().string(), true);
        progressBar.finish();
    }

    in.close();
    out.close();

    FileAttributePreserver::preserve(src, dest, options.hpuxExtentAcl);

    fs::remove(src, ec);
    if (ec) {
        std::cerr << "mv: cannot unlink source \"" << src.string() << "\": " << ec.message() << "\n";
        return false;
    }

    if (options.outputJson) {
        std::cout << "{\"action\":\"move\",\"source\":\"" << JsonSerializer::escape(src.string())
                  << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) << "\"}\n";
    }

    return true;
}

bool MoveEngine::moveDirectoryCrossVolume(const fs::path& src, const fs::path& dest) {
    std::error_code ec;
    fs::create_directories(dest, ec);

    for (const auto& entry : fs::directory_iterator(src, fs::directory_options::skip_permission_denied)) {
        const fs::path& currentPath = entry.path();
        fs::path targetPath = dest / currentPath.filename();

        if (entry.is_directory()) {
            if (!moveDirectoryCrossVolume(currentPath, targetPath)) {
                return false;
            }
        } else {
            if (!copyAndUnlinkFile(currentPath, targetPath)) {
                return false;
            }
        }
    }

    fs::remove(src, ec);
    return true;
}

bool MoveEngine::moveItem(const fs::path& src, const fs::path& dest) {
    bool replaceTarget = false;
    if (!prepareDestination(dest, replaceTarget)) {
        std::cout << "mv: skipping \"" << src.string() << "\"\n";
        return true;
    }

    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (replaceTarget) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }

    if (MoveFileExW(src.c_str(), dest.c_str(), flags)) {
        if (options.verbose) {
            std::cout << "renamed: " << src.string() << " -> " << dest.string() << "\n";
        }
        if (options.outputJson) {
            std::cout << "{\"action\":\"move\",\"source\":\"" << JsonSerializer::escape(src.string())
                      << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) << "\"}\n";
        }
        return true;
    }

    DWORD error = GetLastError();

    if (error == ERROR_NOT_SAME_DEVICE) {
        if (fs::is_directory(src)) {
            return moveDirectoryCrossVolume(src, dest);
        } else {
            return copyAndUnlinkFile(src, dest);
        }
    }

    std::cerr << "mv: cannot move \"" << src.string() << "\" to \"" 
              << dest.string() << "\": System Error " << error << "\n";
    return false;
}

bool MoveEngine::execute() {
    if (options.sources.empty()) {
        std::cerr << "mv: missing file operand\nTry 'mv --help' for more information.\n";
        return false;
    }

    bool destIsDir = fs::is_directory(options.destination);

    if (options.sources.size() > 1 && !destIsDir) {
        std::cerr << "mv: target \"" << options.destination.string() 
                  << "\" is not a directory\n";
        return false;
    }

    for (const auto& src : options.sources) {
        if (!fs::exists(src)) {
            std::cerr << "mv: cannot stat \"" << src.string() << "\": No such file or directory\n";
            return false;
        }

        fs::path targetPath = destIsDir ? (options.destination / src.filename()) : options.destination;

        if (!moveItem(src, targetPath)) {
            return false;
        }
    }
    return true;
}
