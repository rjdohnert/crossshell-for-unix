#ifndef MEMINFO_REPORTER_HPP
#define MEMINFO_REPORTER_HPP

#include "meminfo.hpp"

class OutputFormatter {
protected:
    std::string FormatBytes(uint64_t bytes, DisplayUnit unit) const;
    double Percent(uint64_t part, uint64_t total) const;
    std::string Narrow(const std::wstring& wide) const;

public:
    virtual ~OutputFormatter() = default;
    virtual void Render(std::ostream& os, const MemorySnapshot& snap, const Config& cfg) = 0;
};

class TableFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config& cfg) override;
};

class JsonFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config&) override;
};

class CsvFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config&) override;
};

#endif // MEMINFO_REPORTER_HPP
