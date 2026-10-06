#include "priority_mapper.hpp"

std::optional<DWORD> PriorityMapper::fromNiceValue(int nice) noexcept {
        if (nice < -20 || nice > 20) return std::nullopt;
        if (nice <= -15) return REALTIME_PRIORITY_CLASS;
        if (nice <= -6)  return HIGH_PRIORITY_CLASS;
        if (nice <= -1)  return ABOVE_NORMAL_PRIORITY_CLASS;
        if (nice == 0)   return NORMAL_PRIORITY_CLASS;
        if (nice <= 9)   return BELOW_NORMAL_PRIORITY_CLASS;
        return IDLE_PRIORITY_CLASS;
    }

std::optional<DWORD> PriorityMapper::fromString(std::string_view name) noexcept {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lower == "realtime" || lower == "rt")       return REALTIME_PRIORITY_CLASS;
        if (lower == "high" || lower == "h")           return HIGH_PRIORITY_CLASS;
        if (lower == "abovenormal" || lower == "an")   return ABOVE_NORMAL_PRIORITY_CLASS;
        if (lower == "normal" || lower == "n")         return NORMAL_PRIORITY_CLASS;
        if (lower == "belownormal" || lower == "bn")   return BELOW_NORMAL_PRIORITY_CLASS;
        if (lower == "idle" || lower == "low" || lower == "i") return IDLE_PRIORITY_CLASS;

        return std::nullopt;
    }

std::string PriorityMapper::toString(DWORD priorityClass) {
        switch (priorityClass) {
            case REALTIME_PRIORITY_CLASS:     return "Realtime (-20)";
            case HIGH_PRIORITY_CLASS:         return "High (-10)";
            case ABOVE_NORMAL_PRIORITY_CLASS: return "AboveNormal (-5)";
            case NORMAL_PRIORITY_CLASS:       return "Normal (0)";
            case BELOW_NORMAL_PRIORITY_CLASS: return "BelowNormal (5)";
            case IDLE_PRIORITY_CLASS:         return "Idle (15)";
            default:                          return "Unknown";
        }
    }
