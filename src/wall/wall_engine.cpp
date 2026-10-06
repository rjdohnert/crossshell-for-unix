#include "banner_formatter.hpp"
#include "session_broadcaster.hpp"
#include "system_info_provider.hpp"
#include "wall_config.hpp"
#include "wall_engine.hpp"

WallEngine::WallEngine(WallConfig cfg) : config(std::move(cfg)) {}

int WallEngine::execute() {
        std::string rawMessage;

        if (!config.inlineMessage.empty()) {
            rawMessage = config.inlineMessage;
        } else if (!config.filePath.empty()) {
            std::ifstream file(config.filePath);
            if (!file.is_open()) {
                std::cerr << "wall: cannot open file " << config.filePath << "\n";
                return 1;
            }
            std::ostringstream ss;
            ss << file.rdbuf();
            rawMessage = ss.str();
        } else {
            std::string line;
            std::ostringstream ss;
            while (std::getline(std::cin, line)) {
                ss << line << "\r\n";
            }
            rawMessage = ss.str();
        }

        if (rawMessage.empty()) {
            std::cerr << "wall: empty message\n";
            return 1;
        }

        std::string user = SystemInfoProvider::getCurrentUserName();
        std::string host = SystemInfoProvider::getCurrentHostName();
        std::string timeStr = SystemInfoProvider::getCurrentTimestamp();

        std::string fullBroadcast;
        if (!config.noBanner) {
            fullBroadcast = BannerFormatter::format(user, host, timeStr) + rawMessage + "\r\n";
        } else {
            fullBroadcast = "\r\n" + rawMessage + "\r\n";
        }

        HANDLE hServer = WTS_CURRENT_SERVER_HANDLE;
        if (!config.serverName.empty()) {
            std::wstring wServer = SystemInfoProvider::utf8ToWide(config.serverName);
            hServer = WTSOpenServerW(wServer.data());
            if (!hServer) {
                std::cerr << "wall: failed to open server " << config.serverName << "\n";
                return 1;
            }
        }

        std::string title = "Broadcast from " + user + "@" + host;
        bool ok = SessionBroadcaster::broadcastWts(hServer, title, fullBroadcast, config.guiPopup);

        if (!ok || config.useConhost) {
            std::cout << fullBroadcast;
            std::cout.flush();
        }

        if (hServer != WTS_CURRENT_SERVER_HANDLE && hServer != NULL) {
            WTSCloseServer(hServer);
        }

        if (config.outputFormat != 0 || !config.pipeCommand.empty()) {
            std::string text;
            if (config.outputFormat == 1) {
                text = "{\"status\":\"broadcast_sent\",\"sender\":\"" + user + "\",\"host\":\"" + host + "\"}\n";
            } else if (config.outputFormat == 2) {
                text = "\"status\",\"sender\",\"host\"\n\"broadcast_sent\",\"" + user + "\",\"" + host + "\"\n";
            } else if (config.outputFormat == 3) {
                text = "STATUS\tSENDER\tHOST\n-----------------------------\nbroadcast_sent\t" + user + "\t" + host + "\n";
            }

            if (!config.pipeCommand.empty()) {
                FILE* pipe = _popen(config.pipeCommand.c_str(), "w");
                if (pipe) {
                    std::fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else if (config.outputFormat != 0) {
                std::cout << text;
            }
        }

        return 0;
    }
