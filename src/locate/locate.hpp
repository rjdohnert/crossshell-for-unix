#ifndef LOCATE_HPP
#define LOCATE_HPP

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

inline std::string pathToUtf8(const fs::path& p) {
#if defined(_WIN32)
    auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
#else
    return p.string();
#endif
}

struct FindContext {
    fs::directory_entry entry;
    int depth;
    fs::path start_path;
};

class Expression {
public:
    virtual ~Expression() = default;
    virtual bool evaluate(const FindContext& ctx) = 0;
    virtual bool is_action() const { return false; }
};

#endif // LOCATE_HPP
