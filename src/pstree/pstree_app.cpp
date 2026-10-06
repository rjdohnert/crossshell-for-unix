#include "pstree_app.hpp"

int PstreeApp::run(int argc, wchar_t* argv[]) {
    if (!ProcessTreeEngine::EnableVirtualTerminalAndUnicode()) {
        std::wcerr << L"Warning: Unable to initialize UTF-16 console rendering mode.\n";
    }

    Config config;
    if (!PstreeOptions::ParseCommandLine(argc, argv, config)) {
        return 1;
    }

    ProcessMap processMap;
    if (!ProcessTreeEngine::SnapshotProcesses(processMap)) {
        std::wcerr << L"Error: Failed to obtain system process snapshot (Win32 Error: " 
                   << GetLastError() << L").\n";
        return 2;
    }

    TreeRenderer renderer(config, processMap);
    renderer.Render();

    return 0;
}
