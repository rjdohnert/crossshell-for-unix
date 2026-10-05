/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * CrossShell for UNIX
 */

#include "table.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>

void TableRenderer::addHeader(const std::vector<std::wstring>& headers) {
    m_headers = headers;
}

void TableRenderer::addRow(const std::vector<std::wstring>& row) {
    m_rows.push_back({ row });
}

void TableRenderer::render(std::wostream& os) const {
    if (m_headers.empty()) return;

    size_t colCount = m_headers.size();
    std::vector<size_t> colWidths(colCount, 0);

    for (size_t i = 0; i < colCount; ++i) {
        colWidths[i] = m_headers[i].length();
    }

    for (const auto& row : m_rows) {
        for (size_t i = 0; i < std::min(row.cells.size(), colCount); ++i) {
            colWidths[i] = std::max(colWidths[i], row.cells[i].length());
        }
    }

    for (size_t i = 0; i < colCount; ++i) {
        bool rightAlign = isNumericColumn(i);
        printCell(os, m_headers[i], colWidths[i], rightAlign, i == colCount - 1);
    }
    os << L"\n";

    for (const auto& row : m_rows) {
        for (size_t i = 0; i < colCount; ++i) {
            std::wstring val = (i < row.cells.size()) ? row.cells[i] : L"";
            bool rightAlign = isNumericColumn(i);
            printCell(os, val, colWidths[i], rightAlign, i == colCount - 1);
        }
        os << L"\n";
    }
}

bool TableRenderer::isNumericColumn(size_t index) const {
    if (index == 0) return false;
    if (index == m_headers.size() - 1) return false;
    if (m_headers[index] == L"Type") return false;
    return true;
}

void TableRenderer::printCell(std::wostream& os, const std::wstring& text, size_t width, bool rightAlign, bool isLast) {
    if (isLast && !rightAlign) {
        os << text;
        return;
    }
    if (rightAlign) {
        os << std::setw(static_cast<int>(width)) << text;
    } else {
        os << std::left << std::setw(static_cast<int>(width)) << text << std::right;
    }
    if (!isLast) os << L"  ";
}
