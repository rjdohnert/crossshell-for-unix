#ifndef RSYNC_HPP
#define RSYNC_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <functional>

namespace fs = std::filesystem;

class SyncStatistics {
public:
    uint64_t totalFilesChecked{0};
    uint64_t filesTransferred{0};
    uint64_t filesDeleted{0};
    uint64_t bytesTransferred{0};
    uint64_t totalBytes{0};
    uint64_t deltaBytesSaved{0};
    std::chrono::duration<double> elapsedTime{0};

    void print() const;
    static std::string formatBytes(uint64_t bytes);
};

#endif // RSYNC_HPP
