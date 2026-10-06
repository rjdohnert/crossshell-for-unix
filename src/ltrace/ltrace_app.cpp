#include "ltrace_app.hpp"
#include "engine.hpp"
#include <iostream>

LtraceApp::LtraceApp(Config cfg) : config(std::move(cfg)) {}

int LtraceApp::run() {
    FILE* outputPipe = nullptr;
    std::streambuf* oldOutputBuffer = nullptr;
    PipeStreambuf* pipeBuffer = nullptr;

    if (!config.pipeCommand.empty()) {
        outputPipe = _popen(config.pipeCommand.c_str(), "w");
        if (!outputPipe) {
            std::cerr << "Error: Failed to start pipe command '" << config.pipeCommand << "'\n";
            return 1;
        }
        pipeBuffer = new PipeStreambuf(outputPipe);
        oldOutputBuffer = std::cout.rdbuf(pipeBuffer);
    }

    LibraryTracer tracer(config);
    bool ok = tracer.run();

    if (oldOutputBuffer) std::cout.rdbuf(oldOutputBuffer);
    if (pipeBuffer) {
        delete pipeBuffer;
        _pclose(outputPipe);
    }

    return ok ? 0 : 1;
}
