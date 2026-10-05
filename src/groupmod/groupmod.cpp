/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <lm.h>
#include <io.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <memory>
#include <iomanip>
#include <optional>

#pragma comment(lib, "netapi32.lib")

namespace Utility {

#include "groupmod_module_01_converter.inc"
#include "groupmod_module_02_options.inc"
#include "groupmod_module_03_manager.inc"
#include "groupmod_module_04_parser.inc"
#include "groupmod_module_05_app.inc"

} // namespace Utility

int main(int argc, char* argv[]) {
    try {
        auto options = Utility::CommandLineParser::Parse(argc, argv);
        Utility::GroupModApplication app(std::move(options));
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "groupmod: fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "groupmod: unknown fatal error occurred.\n";
        return 1;
    }
}