#pragma once

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>

class AtomicOutput {
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

public:
    AtomicOutput();
    ~AtomicOutput();

    AtomicOutput(const AtomicOutput&) = delete;
    AtomicOutput& operator=(const AtomicOutput&) = delete;

    bool open(const std::filesystem::path& target, bool allowReplace,
              std::filesystem::perms permissions, std::string& error);
    std::ostream& stream();
    bool commit(std::string& error);
};
