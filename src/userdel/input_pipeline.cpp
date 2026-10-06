#include "delete_options.hpp"
#include "input_pipeline.hpp"
#include "string_utils.hpp"
#include "windows_user_manager.hpp"

bool InputPipeline::IsPipeActive() {
        return !_isatty(_fileno(stdin));
    }

int InputPipeline::ProcessBatchPipe(const DeleteOptions& baseOpt) {
        std::string line;
        size_t count = 0;
        size_t successCount = 0;

        while (std::getline(std::cin, line)) {
            line = StringUtils::Trim(line);
            if (line.empty() || line[0] == '#') continue;

            // Handle comma or whitespace separated lines
            std::vector<std::string> userList;
            if (line.find(',') != std::string::npos) {
                userList = StringUtils::Split(line, ',');
            } else if (line.find(':') != std::string::npos) {
                userList = StringUtils::Split(line, ':');
            } else {
                userList.push_back(line);
            }

            for (const auto& rawUser : userList) {
                std::string targetUser = StringUtils::Trim(rawUser);
                if (targetUser.empty()) continue;

                count++;
                DeleteOptions opt = baseOpt;
                opt.username = targetUser;

                std::string err;
                if (WindowsUserManager::DeleteAccount(opt, err)) {
                    successCount++;
                    if (baseOpt.verbose) {
                        std::cout << "[SUCCESS] Deleted user: " << targetUser << "\n";
                    }
                } else {
                    std::cerr << "[ERROR] Failed deleting user '" << targetUser << "': " << err << "\n";
                }
            }
        }

        std::cout << "Batch deletion finished: " << successCount << "/" << count << " accounts deleted.\n";
        return (successCount == count) ? 0 : 1;
    }
