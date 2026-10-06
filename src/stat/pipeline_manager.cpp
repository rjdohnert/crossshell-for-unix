#include "pipeline_manager.hpp"

bool PipelineManager::isInputPiped() {
        return _isatty(_fileno(stdin)) == 0;
    }

std::vector<std::wstring> PipelineManager::readPipedPaths() {
        std::vector<std::wstring> paths;
        std::wstring line;
        while (std::getline(std::wcin, line)) {
            // Trim whitespace / quotes
            line.erase(0, line.find_first_not_of(L" \t\r\n\""));
            line.erase(line.find_last_not_of(L" \t\r\n\"") + 1);
            if (!line.empty()) {
                paths.push_back(line);
            }
        }
        return paths;
    }
