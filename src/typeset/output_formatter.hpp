#pragma once

#include "typeset.hpp"
#include "variable_attributes.hpp"

class OutputFormatter {
private:
    OutputFormat m_format{OutputFormat::Default};
    bool m_headerEmitted{false};

public:
    explicit OutputFormatter(OutputFormat fmt);

    void EmitRecord(const std::wstring& name, const std::wstring& value, const VariableAttributes& attr = {});
};
