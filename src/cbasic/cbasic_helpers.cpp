#include "cbasic_common.hpp"
#include <cmath>
#include <iostream>
#include <sstream>


fs::path g_virtualRoot;

bool isPathWithinRoot(const fs::path& root, const fs::path& target) {
    auto rootIt = root.begin();
    auto targetIt = target.begin();
    for (; rootIt != root.end(); ++rootIt, ++targetIt) {
        if (targetIt == target.end() || *rootIt != *targetIt) {
            return false;
        }
    }
    return true;
}

bool resolveSandboxPath(const std::string& inputPath, std::string& resolvedOut, std::string& errorOut) {
    if (inputPath.empty()) {
        errorOut = "Empty path is not allowed.";
        return false;
    }

    std::error_code ec;
    fs::path root = fs::absolute(g_virtualRoot, ec).lexically_normal();
    if (ec) {
        errorOut = "Sandbox root is unavailable.";
        return false;
    }
    root = fs::weakly_canonical(root, ec);
    if (ec) {
        errorOut = "Sandbox root is unavailable.";
        return false;
    }

    fs::path requested(inputPath);
    fs::path combined = requested.is_absolute() ? requested : (root / requested);
    fs::path target = fs::absolute(combined, ec).lexically_normal();
    if (ec) {
        errorOut = "Invalid path.";
        return false;
    }
    target = fs::weakly_canonical(target, ec);
    if (ec) {
        errorOut = "Invalid path.";
        return false;
    }

    if (!isPathWithinRoot(root, target)) {
        errorOut = "Access denied outside sandbox root.";
        return false;
    }

    resolvedOut = target.string();
    return true;
}

std::string getSandboxRootDisplay() {
    std::error_code ec;
    fs::path root = fs::absolute(g_virtualRoot, ec).lexically_normal();
    if (ec) return g_virtualRoot.string();
    return root.string();
}

double asDouble(const Value& val) {
    if (std::holds_alternative<double>(val)) return std::get<double>(val);
    try {
        return std::stod(std::get<std::string>(val));
    } catch (...) {
        return 0.0;
    }
}

std::string valueToString(const Value& val) {
    if (std::holds_alternative<double>(val)) {
        double d = std::get<double>(val);
        if (std::floor(d) == d && !std::isinf(d) && !std::isnan(d)) {
            return std::to_string(static_cast<long long>(d));
        }
        std::ostringstream oss;
        oss << d;
        return oss.str();
    }
    return std::get<std::string>(val);
}

bool valueToBool(const Value& val) {
    if (std::holds_alternative<double>(val)) return std::get<double>(val) != 0.0;
    return !std::get<std::string>(val).empty();
}

std::string ensureExtension(std::string filename, const std::string& defaultExt) {
    size_t first = filename.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = filename.find_last_not_of(" \t\r\n");
    filename = filename.substr(first, (last - first + 1));

    if (filename.length() >= 2 && filename.front() == '"' && filename.back() == '"') {
        filename = filename.substr(1, filename.length() - 2);
    }

    first = filename.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    last = filename.find_last_not_of(" \t\r\n");
    filename = filename.substr(first, (last - first + 1));

    if (filename.find_last_of('.') == std::string::npos) {
        filename += defaultExt;
    }
    return filename;
}

void printHelp();
void printVersion();

void printErrorLine(const std::string& message) {
    std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
}

