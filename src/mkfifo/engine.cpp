#include "engine.hpp"

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

DWORD WindowsPipeMode::getOpenMode() const {
    DWORD mode = FILE_FLAG_FIRST_PIPE_INSTANCE;
    switch (direction) {
        case Direction::Inbound:  mode |= PIPE_ACCESS_INBOUND; break;
        case Direction::Outbound: mode |= PIPE_ACCESS_OUTBOUND; break;
        case Direction::Duplex:
        default:                  mode |= PIPE_ACCESS_DUPLEX; break;
    }
    return mode;
}

DWORD WindowsPipeMode::getPipeMode() const {
    DWORD mode = PIPE_WAIT;
    switch (type) {
        case Type::Message: mode |= (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE); break;
        case Type::Byte:
        default:            mode |= (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE); break;
    }
    return mode;
}

std::string WindowsPipeMode::getDirectionString() const {
    switch (direction) {
        case Direction::Inbound:  return "PIPE_ACCESS_INBOUND (0x00000001)";
        case Direction::Outbound: return "PIPE_ACCESS_OUTBOUND (0x00000002)";
        case Direction::Duplex:   return "PIPE_ACCESS_DUPLEX (0x00000003)";
    }
    return "UNKNOWN";
}

std::string WindowsPipeMode::getTypeString() const {
    switch (type) {
        case Type::Message: return "PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE (0x00000006)";
        case Type::Byte:    return "PIPE_TYPE_BYTE | PIPE_READMODE_BYTE (0x00000000)";
    }
    return "UNKNOWN";
}

SecurityDescriptorManager::SecurityDescriptorManager(const std::string& sddl) {
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
            sddl.c_str(), SDDL_REVISION_1, &pSD, nullptr)) {
        std::cerr << "mkfifo: warning: invalid SDDL string '" << sddl 
                  << "', falling back to default security.\n";
        sa.lpSecurityDescriptor = nullptr;
    } else {
        sa.lpSecurityDescriptor = pSD;
    }
}

SecurityDescriptorManager::~SecurityDescriptorManager() {
    if (pSD) {
        LocalFree(pSD);
    }
}

LPSECURITY_ATTRIBUTES SecurityDescriptorManager::getAttributes() {
    return &sa;
}

std::string NamedPipeInstance::normalizeName(const std::string& input) {
    if (input.rfind(R"(\\.\pipe\)", 0) == 0) {
        return input;
    }
    return R"(\\.\pipe\)" + input;
}

NamedPipeInstance::NamedPipeInstance(const std::string& name, const WindowsPipeMode& m, uint32_t bSize)
    : fullPath(normalizeName(name)), mode(m), bufSize(bSize) {}

NamedPipeInstance::~NamedPipeInstance() {
    close();
}

bool NamedPipeInstance::create(SecurityDescriptorManager& secMgr, bool verbose) {
    hPipe = CreateNamedPipeA(
        fullPath.c_str(),
        mode.getOpenMode(),
        mode.getPipeMode(),
        PIPE_UNLIMITED_INSTANCES,
        bufSize,
        bufSize,
        0,
        secMgr.getAttributes()
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::cerr << "mkfifo: cannot create FIFO '" << fullPath << "': ";
        if (err == ERROR_ALREADY_EXISTS || err == ERROR_PIPE_BUSY) {
            std::cerr << "File exists (Pipe already active in NPFS)\n";
        } else if (err == ERROR_ACCESS_DENIED) {
            std::cerr << "Access denied\n";
        } else {
            std::cerr << "System Error " << err << "\n";
        }
        return false;
    }

    if (verbose) {
        std::cout << "FIFO Special Object Created in Windows NPFS:\n"
                  << "  Path:        " << fullPath << "\n"
                  << "  Access Mode: " << mode.getDirectionString() << "\n"
                  << "  Pipe Type:   " << mode.getTypeString() << "\n"
                  << "  Buffer Size: " << bufSize << " bytes\n"
                  << "  DACL (SDDL): " << mode.sddlString << "\n";
    }
    return true;
}

void NamedPipeInstance::close() {
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
}

const std::string& NamedPipeInstance::getPath() const {
    return fullPath;
}

FifoEngine::FifoEngine(const FifoOptions& opts) : options(opts) {}

int FifoEngine::execute() {
    SecurityDescriptorManager secMgr(options.mode.sddlString);
    std::vector<std::unique_ptr<NamedPipeInstance>> pipes;

    for (const auto& name : options.pipeNames) {
        auto pipe = std::make_unique<NamedPipeInstance>(name, options.mode, options.bufferSize);
        if (!pipe->create(secMgr, options.verbose)) {
            return 1;
        }
        pipes.push_back(std::move(pipe));
    }

    if (options.persist) {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "mkfifo: " << pipes.size() << " FIFO pipe(s) active in NPFS. Press Ctrl+C to release.\n";
        while (g_running) {
            Sleep(200);
        }
        std::cout << "\nmkfifo: releasing FIFO handles and closing endpoints.\n";
    }

    return 0;
}
