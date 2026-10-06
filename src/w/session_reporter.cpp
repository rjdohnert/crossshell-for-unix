#include "output_quoting.hpp"
#include "pipe_buffer.hpp"
#include "session_data.hpp"
#include "session_reporter.hpp"
#include "session_time_formatter.hpp"
#include "string_encoding.hpp"
#include "summary_header.hpp"

int reportUserSessions(const std::vector<SessionData>& userSessions, bool showHeader, bool shortFormat, bool showFrom, OutputFormat outputFormat) {
    if (showHeader && outputFormat == OutputFormat::Human) {
        PrintHeader(userSessions.size());
    }

    if (userSessions.empty()) {
        return 0;
    }

    if (outputFormat == OutputFormat::Csv) std::cout << "user,tty,from,login,idle,jcpu,pcpu,what\n";
    if (outputFormat == OutputFormat::Tsv) std::cout << "USER\tTTY\tFROM\tLOGIN\tIDLE\tJCPU\tPCPU\tWHAT\n";
    if (outputFormat == OutputFormat::Table) std::cout << "USER\tTTY\tFROM\tLOGIN\tIDLE\tJCPU\tPCPU\tWHAT\n";

    // Print Column Headers
    if (outputFormat != OutputFormat::Human) {
        for (const auto& s : userSessions) {
            std::string u = WStrToStr(s.username), t = WStrToStr(s.tty), f = WStrToStr(s.fromHost), w = WStrToStr(s.whatProcess);
            std::string l = FormatLoginTime(s.logonTime), idle = FormatIdleTime(s.idleTimeMs), j = FormatCpuTime(s.jcpu100ns), p = FormatCpuTime(s.pcpu100ns);
            if (outputFormat == OutputFormat::Json) {
                std::cout << "{\"user\":" << JsonQuote(u) << ",\"tty\":" << JsonQuote(t) << ",\"from\":" << JsonQuote(f) << ",\"login\":" << JsonQuote(l) << ",\"idle\":" << JsonQuote(idle) << ",\"jcpu\":" << JsonQuote(j) << ",\"pcpu\":" << JsonQuote(p) << ",\"what\":" << JsonQuote(w) << "}\n";
            } else if (outputFormat == OutputFormat::Csv) {
                std::cout << CsvQuote(u) << ',' << CsvQuote(t) << ',' << CsvQuote(f) << ',' << CsvQuote(l) << ',' << CsvQuote(idle) << ',' << CsvQuote(j) << ',' << CsvQuote(p) << ',' << CsvQuote(w) << "\n";
            } else {
                std::cout << u << '\t' << t << '\t' << f << '\t' << l << '\t' << idle << '\t' << j << '\t' << p << '\t' << w << "\n";
            }
        }
        return 0;
    }

    std::cout << std::left << std::setw(12) << "User"
              << std::setw(12) << "TTY";

    if (showFrom) {
        std::cout << std::setw(18) << "FROM";
    }

    if (!shortFormat) {
        std::cout << std::setw(10) << "Login@"
                  << std::setw(8)  << "Idle"
                  << std::setw(9)  << "JCPU"
                  << std::setw(9)  << "PCPU";
    } else {
        std::cout << std::setw(8) << "Idle";
    }

    std::cout << "WHAT\n";

    // Print User Session Rows
    for (const auto& s : userSessions) {
        std::string uStr = WStrToStr(s.username);
        std::string tStr = WStrToStr(s.tty);
        std::string fStr = WStrToStr(s.fromHost);
        std::string lStr = FormatLoginTime(s.logonTime);
        std::string iStr = FormatIdleTime(s.idleTimeMs);
        std::string jStr = FormatCpuTime(s.jcpu100ns);
        std::string pStr = FormatCpuTime(s.pcpu100ns);
        std::string wStr = WStrToStr(s.whatProcess);

        std::cout << std::left << std::setw(12) << uStr
                  << std::setw(12) << tStr;

        if (showFrom) {
            std::cout << std::setw(18) << fStr;
        }

        if (!shortFormat) {
            std::cout << std::setw(10) << lStr
                      << std::setw(8)  << iStr
                      << std::setw(9)  << jStr
                      << std::setw(9)  << pStr;
        } else {
            std::cout << std::setw(8) << iStr;
        }

        std::cout << wStr << "\n";
    }

    return 0;
}

