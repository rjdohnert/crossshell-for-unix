#include "signal_parser.hpp"

std::string SignalParser::ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return s;
    }

bool SignalParser::Parse(const std::string& arg, SignalType& outSignal) {
        std::string sigStr = arg;
        if (!sigStr.empty() && sigStr[0] == '-') {
            sigStr = sigStr.substr(1);
        }
        sigStr = ToUpper(sigStr);

        if (sigStr.rfind("SIG", 0) == 0) {
            sigStr = sigStr.substr(3);
        }

        if (sigStr == "1" || sigStr == "HUP") { outSignal = SignalType::SIGHUP; return true; }
        if (sigStr == "2" || sigStr == "INT" || sigStr == "CTRLC") { outSignal = SignalType::SIGINT; return true; }
        if (sigStr == "3" || sigStr == "QUIT" || sigStr == "CTRLBREAK") { outSignal = SignalType::SIGQUIT; return true; }
        if (sigStr == "9" || sigStr == "STOP") { outSignal = SignalType::SIGstop; return true; }
        if (sigStr == "15" || sigStr == "TERM") { outSignal = SignalType::SIGTERM; return true; }

        return false;
    }

void SignalParser::PrintSignalList() {
        std::cout << " 1) SIGHUP       2) SIGINT       3) SIGQUIT      9) SIGstop\n"
                  << "15) SIGTERM\n";
    }
