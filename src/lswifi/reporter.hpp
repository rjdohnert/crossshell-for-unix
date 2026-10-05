#pragma once

#include "lswifi.hpp"
#include "options.hpp"
#include <memory>

class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) = 0;
};

class TableFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override;
private:
    static std::string RenderSignalBar(uint32_t quality, bool color);
};

class JsonFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override;
};

class CsvFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override;
};

class FormatterFactory {
public:
    static std::unique_ptr<IFormatter> Create(RuntimeConfig::Format format);
};
