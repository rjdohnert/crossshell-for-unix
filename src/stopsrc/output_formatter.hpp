#pragma once

#include "stopsrc.hpp"

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class OutputFormatter {
private:
    OutputFormat m_format{OutputFormat::Default};
    std::wstring m_pipeCommand;

public:
    OutputFormatter(OutputFormat fmt, std::wstring pipeCmd);

    void Emit(const std::wstring& message) const;
};
