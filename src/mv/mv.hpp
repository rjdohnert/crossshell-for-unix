#ifndef MV_HPP
#define MV_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

class JsonSerializer {
public:
    static std::string escape(const std::string& str);
};

class MoveOptions {
public:
    bool force = false;
    bool interactive = false;
    bool hpuxExtentAcl = false;
    bool showProgress = true;
    bool verbose = false;
    bool outputJson = false;
    bool showHelp = false;
    bool showVersion = false;

    std::vector<fs::path> sources;
    fs::path destination;
};

class ProgressBar {
private:
    uint64_t totalBytes = 0;
    uint64_t transferredBytes = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRenderTime;
    int barWidth = 30;
    bool cursorHidden = false;

    static std::string formatSize(uint64_t bytes);
    static std::string formatTime(uint64_t seconds);
    void hideCursor();
    void showCursor();

public:
    ProgressBar();
    ~ProgressBar();
    void reset(uint64_t fileBytes);
    void update(uint64_t chunkBytes, const std::string& currentFileName, bool forceRender = false);
    void finish();
};

class FileAttributePreserver {
public:
    static void preserve(const fs::path& src, const fs::path& dest, bool preserveExtended);
};

#endif // MV_HPP
