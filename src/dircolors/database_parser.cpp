/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "database_parser.hpp"
#include <sstream>

namespace dircolors {

const char* ColorDatabaseParser::DEFAULT_DATABASE = R"(
# Default dircolors configuration for Windows
RESET 0
DIR 01;34
LINK 01;36
EXEC 01;32
FIFO 40;33
SOCK 01;35
BLK 40;33;01
CHR 40;33;01
ORPHAN 40;31;01
MISSING 00

# Executables & Scripts on Windows
.exe 01;32
.bat 01;32
.cmd 01;32
.ps1 01;32
.com 01;32
.msi 01;32

# Archives / Compressed files
.tar 01;31
.tgz 01;31
.zip 01;31
.z 01;31
.gz 01;31
.bz2 01;31
.xz 01;31
.7z 01;31
.rar 01;31

# Documents
.pdf 00;32
.doc 00;32
.docx 00;32
.xls 00;32
.xlsx 00;32
.ppt 00;32
.pptx 00;32

# Images & Media
.jpg 01;35
.jpeg 01;35
.png 01;35
.gif 01;35
.bmp 01;35
.svg 01;35
.mp3 00;36
.wav 00;36
.mp4 01;35
.mkv 01;35
.avi 01;35
)";

const std::unordered_map<std::string, std::string> ColorDatabaseParser::KEY_MAP = {
    {"RESET", "rs"}, {"DIR", "di"}, {"LINK", "ln"}, {"MULTIHARDLINK", "mh"},
    {"FIFO", "fi"}, {"SOCK", "so"}, {"DOOR", "do"}, {"BLK", "bd"},
    {"CHR", "cd"}, {"ORPHAN", "or"}, {"MISSING", "mi"}, {"SETUID", "su"},
    {"SETGID", "sg"}, {"CAPABILITY", "ca"}, {"STICKY_OTHER_WRITABLE", "tw"},
    {"OTHER_WRITABLE", "ow"}, {"STICKY", "st"}, {"EXEC", "ex"}, {"FILE", "fi"}
};

const char* ColorDatabaseParser::getDefaultDatabase() {
    return DEFAULT_DATABASE;
}

std::string ColorDatabaseParser::parseDatabase(std::istream& in) {
    std::string line;
    std::string lsColors;

    while (std::getline(in, line)) {
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);

        std::istringstream iss(line);
        std::string key, val;
        if (!(iss >> key >> val)) continue;

        std::string code;
        auto it = KEY_MAP.find(key);
        if (it != KEY_MAP.end()) {
            code = it->second + "=" + val;
        } else if (!key.empty() && key[0] == '.') {
            code = "*" + key + "=" + val;
        } else {
            continue;
        }

        if (!lsColors.empty()) lsColors += ":";
        lsColors += code;
    }

    return lsColors;
}

} // namespace dircolors
