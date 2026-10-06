#include "uptime_calculator.hpp"

std::string UptimeCalculator::getUptimeString() {
        ULONGLONG ms = GetTickCount64();
        ULONGLONG seconds = ms / 1000;
        ULONGLONG minutes = seconds / 60;
        ULONGLONG hours = minutes / 60;
        ULONGLONG days = hours / 24;

        minutes %= 60;
        hours %= 24;

        std::stringstream ss;
        ss << "up ";
        if (days > 0) {
            ss << days << (days == 1 ? " day, " : " days, ");
            ss << std::setw(2) << std::setfill('0') << hours << ":"
               << std::setw(2) << std::setfill('0') << minutes;
        } else if (hours > 0) {
            ss << hours << ":" << std::setw(2) << std::setfill('0') << minutes;
        } else {
            ss << minutes << (minutes == 1 ? " min" : " mins");
        }
        return ss.str();
    }

std::string UptimeCalculator::getPrettyUptimeString() {
        ULONGLONG ms = GetTickCount64();
        ULONGLONG seconds = ms / 1000;
        ULONGLONG minutes = seconds / 60;
        ULONGLONG hours = minutes / 60;
        ULONGLONG days = hours / 24;

        minutes %= 60;
        hours %= 24;

        std::ostringstream ss;
        ss << "up ";
        if (days > 0) {
            ss << days << (days == 1 ? " day" : " days");
            if (hours > 0 || minutes > 0) {
                ss << ", ";
                if (hours > 0) {
                    ss << hours << (hours == 1 ? " hour" : " hours");
                }
                if (minutes > 0) {
                    if (hours > 0) ss << ", ";
                    ss << minutes << (minutes == 1 ? " min" : " mins");
                }
            }
        } else if (hours > 0) {
            ss << hours << (hours == 1 ? " hour" : " hours");
            if (minutes > 0) {
                ss << ", " << minutes << (minutes == 1 ? " min" : " mins");
            }
        } else if (minutes > 0) {
            ss << minutes << (minutes == 1 ? " min" : " mins");
        } else {
            ss << "0 min";
        }
        return ss.str();
    }

std::string UptimeCalculator::getBootTimeString() {
        ULONGLONG ms = GetTickCount64();
        time_t now = time(nullptr);
        time_t bootTime = now - static_cast<time_t>(ms / 1000);

        tm ltm;
        localtime_s(&ltm, &bootTime);

        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ltm);
        return std::string(buf);
    }

std::string UptimeCalculator::getCurrentTimeFormatted() {
        time_t now = time(nullptr);
        tm ltm;
        localtime_s(&ltm, &now);

        char buf[32];
        strftime(buf, sizeof(buf), "%I:%M %p", &ltm);

        std::string s(buf);
        if (!s.empty() && s[0] == '0') {
            s = s.substr(1);
        }
        return s;
    }
