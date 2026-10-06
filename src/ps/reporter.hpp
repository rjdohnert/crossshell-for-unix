#pragma once

#include "ps.hpp"
#include "engine.hpp"

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) = 0;
};

class TableFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override;
};

class CsvFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override;
};

class JsonFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override;

private:
    static std::wstring escapeJson(const std::wstring& s);
};

class XmlFormatter : public IOutputFormatter {
public:
    void render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) override;

private:
    static std::wstring escapeXml(const std::wstring& s);
};
