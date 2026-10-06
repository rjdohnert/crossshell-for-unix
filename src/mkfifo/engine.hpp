#ifndef MKFIFO_ENGINE_HPP
#define MKFIFO_ENGINE_HPP

#include "mkfifo.hpp"

class SecurityDescriptorManager {
private:
    PSECURITY_DESCRIPTOR pSD = nullptr;
    SECURITY_ATTRIBUTES sa;

public:
    explicit SecurityDescriptorManager(const std::string& sddl);
    ~SecurityDescriptorManager();
    LPSECURITY_ATTRIBUTES getAttributes();
};

class NamedPipeInstance {
private:
    std::string fullPath;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    WindowsPipeMode mode;
    uint32_t bufSize;

    static std::string normalizeName(const std::string& input);

public:
    NamedPipeInstance(const std::string& name, const WindowsPipeMode& m, uint32_t bSize);
    ~NamedPipeInstance();

    bool create(SecurityDescriptorManager& secMgr, bool verbose);
    void close();
    const std::string& getPath() const;
};

class FifoEngine {
private:
    FifoOptions options;

public:
    explicit FifoEngine(const FifoOptions& opts);
    int execute();
};

#endif // MKFIFO_ENGINE_HPP
