#include "user_prompt.hpp"

bool UserPrompt::ConfirmAction(const std::string& target) {
        std::cout << "Are you sure you want to securely wipe '" << target << "'? (y/N): ";
        std::string resp;
        std::getline(std::cin, resp);
        if (resp.empty()) return false;
        std::transform(resp.begin(), resp.end(), resp.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return resp == "y" || resp == "yes";
    }
