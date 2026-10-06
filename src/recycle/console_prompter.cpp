#include "console_prompter.hpp"

bool ConsolePrompter::PromptUser(const fs::path& path) {
        std::wcout << L"Recycle '" << path.wstring() << L"'? (y/n): ";
        std::wstring response;
        std::getline(std::wcin, response);
        return (!response.empty() && (response[0] == L'y' || response[0] == L'Y'));
    }
