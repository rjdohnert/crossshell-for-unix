#include "input_pipeline.hpp"
#include "string_utils.hpp"
#include "user_account.hpp"
#include "windows_user_manager.hpp"

bool InputPipeline::IsPipeActive() {
        // Checks if standard input is redirected/piped
        return !_isatty(_fileno(stdin));
    }

void InputPipeline::ProcessBatchPipe(bool verbose) {
        std::string line;
        size_t count = 0;
        size_t successCount = 0;

        while (std::getline(std::cin, line)) {
            // Remove carriage return if present (Windows CRLF)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty() || line[0] == '#') continue; // Skip comments/empty

            // Format: username:password:comment:group1,group2
            auto tokens = StringUtils::Split(line, ':');
            if (tokens.empty()) continue;

            UserAccount user;
            user.username = tokens[0];

            if (tokens.size() > 1) user.password = tokens[1];
            if (tokens.size() > 2) user.comment = tokens[2];
            if (tokens.size() > 3) {
                user.supplementaryGroups = StringUtils::Split(tokens[3], ',');
            }

            // Defaults
            user.homeDir = "C:\\Users\\" + user.username;
            user.createHomeDir = true;

            count++;
            std::string err;
            if (WindowsUserManager::CreateAccount(user, err)) {
                successCount++;
                if (verbose) {
                    std::cout << "[SUCCESS] Created user: " << user.username << "\n";
                }
            } else {
                std::cerr << "[ERROR] Failed creating user '" << user.username << "': " << err << "\n";
            }
        }

        std::cout << "Batch processing finished: " << successCount << "/" << count << " accounts created.\n";
    }
