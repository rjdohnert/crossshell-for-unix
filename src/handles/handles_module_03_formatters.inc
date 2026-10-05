// ============================================================================
// 3. Formatter Abstraction (Strategy Pattern)
// ============================================================================
class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) = 0;
};

class TableFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        // Compute column widths
        size_t maxNameLen = 12; // "ProcessName" min length
        for (const auto& item : data) {
            maxNameLen = (std::max)(maxNameLen, item.narrowName().length());
        }

        // Header
        os << std::left 
           << std::setw(10) << "PID"
           << std::setw(14) << "Handles"
           << std::setw(maxNameLen) << "ProcessName" 
           << "\n";

        os << std::string(10 + 14 + maxNameLen, '-') << "\n";

        // Rows
        for (const auto& item : data) {
            os << std::left << std::setw(10) << item.pid;
            if (item.accessDenied) {
                os << std::left << std::setw(14) << "<AccessDenied>";
            } else {
                os << std::left << std::setw(14) << item.handleCount;
            }
            os << item.narrowName() << "\n";
        }
    }
};

class CsvFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "PID,HandleCount,ProcessName,AccessDenied\n";
        for (const auto& item : data) {
            os << item.pid << ","
               << item.handleCount << ",\""
               << escapeQuotes(item.narrowName()) << "\","
               << (item.accessDenied ? "true" : "false") << "\n";
        }
    }

private:
    static std::string escapeQuotes(std::string str) {
        size_t pos = 0;
        while ((pos = str.find('\"', pos)) != std::string::npos) {
            str.replace(pos, 1, "\"\"");
            pos += 2;
        }
        return str;
    }
};

class JsonFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "[\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& item = data[i];
            os << "  {\n";
            os << "    \"pid\": " << item.pid << ",\n";
            os << "    \"handleCount\": " << item.handleCount << ",\n";
            os << "    \"processName\": \"" << escapeJson(item.narrowName()) << "\",\n";
            os << "    \"accessDenied\": " << (item.accessDenied ? "true" : "false") << "\n";
            os << "  }" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        os << "]\n";
    }

private:
    static std::string escapeJson(const std::string& str) {
        std::ostringstream ss;
        for (char c : str) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else if (c == '\b') ss << "\\b";
            else if (c == '\f') ss << "\\f";
            else if (c == '\n') ss << "\\n";
            else if (c == '\r') ss << "\\r";
            else if (c == '\t') ss << "\\t";
            else ss << c;
        }
        return ss.str();
    }
};

class PipeFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "PID|HandleCount|ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << "|"
               << (item.accessDenied ? 0 : item.handleCount) << "|"
               << item.narrowName() << "\n";
        }
    }
};

#include <winternl.h>

using NtQuerySystemInformationFn = NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

typedef LONG KPRIORITY;

struct SYSTEM_PROCESS_INFORMATION_ENTRY {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
};

