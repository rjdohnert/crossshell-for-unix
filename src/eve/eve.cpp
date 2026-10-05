/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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

/*
 * Single-file index
 * ---------------------------------------------------------------------------
 * 1. Platform headers and console-control handler
 * 2. Language, token, and cross-line highlight state
 * 3. Syntax highlighter and language detection
 * 4. Editor state, buffers, preferences, undo/redo, and file I/O
 * 5. Help text, color themes, and screen rendering (status bar, split window)
 * 6. Command dispatch: DCL-style abbreviation, EXIT confirmation, and Command: verbs
 * 7. Search/replace, cursor movement, and scrolling
 * 8. Selection, clipboard, FILL/margins, and crash-recovery journaling
 * 9. Mouse input, GOLD key, and learn/execute macros
 * 10. Interactive event loop and program entry point
 * ---------------------------------------------------------------------------
 */

#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "user32.lib")
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

static std::atomic<bool> g_consoleCtrlRequested{false};

static BOOL WINAPI ConsoleCtrlHandler(DWORD controlType) {
    if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT ||
        controlType == CTRL_CLOSE_EVENT || controlType == CTRL_LOGOFF_EVENT ||
        controlType == CTRL_SHUTDOWN_EVENT) {
        g_consoleCtrlRequested.store(true, std::memory_order_relaxed);

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr) {
            INPUT_RECORD ir = { 0 };
            ir.EventType = FOCUS_EVENT;
            ir.Event.FocusEvent.bSetFocus = TRUE;
            DWORD written = 0;
            WriteConsoleInputW(hIn, &ir, 1, &written);
        }
        return TRUE;
    }
    return FALSE;
}

enum class Lang {
    Plain, C_Cpp, Java, Python, Swift, Kotlin, Fortran,
    Cobol, Pascal, JavaScript, Basic, PowerShell, Batch, Shell
};

enum class EveTheme { Default, Black, Orange, Green, Red, Blue, Pink };

enum class SyntaxTokenType : uint8_t {
    Default, Keyword, Type, String, Comment, Number, Directive
};

struct HighlightState {
    char commentTerminator = '\0';
    char quote = '\0';
    bool operator==(const HighlightState& other) const {
        return commentTerminator == other.commentTerminator && quote == other.quote;
    }
};

class Highlighter {
public:
    static bool SupportsSlashBlockComments(Lang lang) {
        return lang == Lang::C_Cpp || lang == Lang::Java || lang == Lang::JavaScript ||
            lang == Lang::Swift || lang == Lang::Kotlin;
    }

    static std::string ToUpper(std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }

    static std::string ToLower(std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    static Lang DetectLanguage(const std::string& filename) {
        size_t dot = filename.find_last_of('.');
        if (dot == std::string::npos) return Lang::Plain;
        std::string ext = ToLower(filename.substr(dot));

        if (ext == ".c" || ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".h" || ext == ".hpp") return Lang::C_Cpp;
        if (ext == ".java") return Lang::Java;
        if (ext == ".py" || ext == ".pyw") return Lang::Python;
        if (ext == ".swift") return Lang::Swift;
        if (ext == ".kt" || ext == ".kts") return Lang::Kotlin;
        if (ext == ".f" || ext == ".for" || ext == ".f90" || ext == ".f95" || ext == ".f03") return Lang::Fortran;
        if (ext == ".cbl" || ext == ".cob" || ext == ".cpy") return Lang::Cobol;
        if (ext == ".pas" || ext == ".pp" || ext == ".inc") return Lang::Pascal;
        if (ext == ".js" || ext == ".jsx" || ext == ".mjs" || ext == ".ts" || ext == ".tsx") return Lang::JavaScript;
        if (ext == ".bas" || ext == ".vb" || ext == ".vbs") return Lang::Basic;
        if (ext == ".ps1" || ext == ".psm1" || ext == ".psd1") return Lang::PowerShell;
        if (ext == ".bat" || ext == ".cmd") return Lang::Batch;
        if (ext == ".sh" || ext == ".bash" || ext == ".zsh" || ext == ".fish") return Lang::Shell;

        return Lang::Plain;
    }

    static std::string GetLangName(Lang lang) {
        switch (lang) {
            case Lang::C_Cpp: return "C/C++";
            case Lang::Java: return "Java";
            case Lang::Python: return "Python";
            case Lang::Swift: return "Swift";
            case Lang::Kotlin: return "Kotlin";
            case Lang::Fortran: return "Fortran";
            case Lang::Cobol: return "COBOL";
            case Lang::Pascal: return "Pascal";
            case Lang::JavaScript: return "JavaScript";
            case Lang::Basic: return "BASIC";
            case Lang::PowerShell: return "PowerShell";
            case Lang::Batch: return "CMD/BAT";
            case Lang::Shell: return "Shell";
            default: return "Text";
        }
    }

    static const char* GetTokenAnsi(SyntaxTokenType t) {
        switch (t) {
            case SyntaxTokenType::Keyword:   return "\x1b[94m"; // Bright Blue
            case SyntaxTokenType::Type:      return "\x1b[96m"; // Bright Cyan
            case SyntaxTokenType::String:    return "\x1b[93m"; // Amber / Yellow
            case SyntaxTokenType::Comment:   return "\x1b[90m"; // Bright Black / Gray
            case SyntaxTokenType::Number:    return "\x1b[95m"; // Bright Magenta
            case SyntaxTokenType::Directive: return "\x1b[35m"; // Magenta
            default:                   return "\x1b[39m"; // Console default foreground
        }
    }

    static std::vector<SyntaxTokenType> HighlightLine(
        const std::string& line, Lang lang, HighlightState state = {}, HighlightState* endState = nullptr) {
        constexpr size_t MAX_HIGHLIGHT_LEN = 8192;
        const size_t n = (std::min)(line.size(), MAX_HIGHLIGHT_LEN);
        std::vector<SyntaxTokenType> tokens(n, SyntaxTokenType::Default);
        auto finish = [&]() {
            if (endState != nullptr) *endState = state;
            return tokens;
        };
        if (lang == Lang::Plain || line.empty() || n == 0) return finish();

        size_t i = 0;

        if (state.commentTerminator != '\0') {
            const std::string terminator = state.commentTerminator == '/' ? "*/" : "}";
            const size_t close = line.find(terminator);
            const size_t covered = close == std::string::npos ? n : (std::min)(n, close + terminator.size());
            std::fill(tokens.begin(), tokens.begin() + covered, SyntaxTokenType::Comment);
            if (close == std::string::npos || close + terminator.size() > n) return finish();
            state.commentTerminator = '\0';
            i = covered;
        }

        if (state.quote != '\0') {
            const char quote = state.quote;
            size_t j = i;
            while (j < n) {
                tokens[j] = SyntaxTokenType::String;
                if (line[j] == '\\' && j + 1 < n) {
                    tokens[++j] = SyntaxTokenType::String;
                } else if (line[j] == quote) {
                    ++j;
                    state.quote = '\0';
                    break;
                }
                ++j;
            }
            if (state.quote != '\0') return finish();
            i = j;
        }

        if (lang == Lang::Fortran && !line.empty() && (line[0] == 'C' || line[0] == 'c' || line[0] == '*')) {
            std::fill(tokens.begin(), tokens.end(), SyntaxTokenType::Comment);
            return finish();
        }
        if (lang == Lang::Cobol && line.size() >= 7 && line[6] == '*') {
            std::fill(tokens.begin(), tokens.end(), SyntaxTokenType::Comment);
            return finish();
        }
        if (lang == Lang::Batch) {
            std::string trimmed = line;
            size_t start = trimmed.find_first_not_of(" \t");
            if (start != std::string::npos) {
                std::string upperStart = ToUpper(trimmed.substr(start, (std::min)(size_t(4), trimmed.size() - start)));
                if (upperStart.rfind("REM", 0) == 0 || upperStart.rfind("::", 0) == 0) {
                    std::fill(tokens.begin(), tokens.end(), SyntaxTokenType::Comment);
                    return finish();
                }
            }
        }

        static const std::unordered_set<std::string> c_kw = {"if","else","for","while","do","switch","case","default","break","continue","return","goto","try","catch","throw","sizeof","decltype","nullptr","true","false","class","struct","enum","union","typedef","namespace","using","new","delete","template","typename","public","private","protected","virtual","override","constexpr","static","const","inline","auto"};
        static const std::unordered_set<std::string> c_ty = {"int","float","double","char","void","bool","long","short","unsigned","signed","size_t","uint8_t","uint16_t","uint32_t","uint64_t","string","vector","map"};
        static const std::unordered_set<std::string> py_kw = {"def","class","import","from","as","return","if","elif","else","for","while","try","except","finally","with","yield","lambda","pass","break","continue","True","False","None","in","is","not","and","or","self","async","await","global","raise"};
        static const std::unordered_set<std::string> js_kw = {"function","const","let","var","return","if","else","for","while","do","switch","case","default","break","continue","class","extends","super","import","export","from","async","await","new","this","typeof","instanceof","true","false","null","undefined","try","catch","finally","throw"};
        static const std::unordered_set<std::string> sh_kw = {"if","then","elif","else","fi","for","in","do","done","while","until","case","esac","function","return","local","export","echo","printf","exit","alias","source","set","fish","end","switch"};
        static const std::unordered_set<std::string> ps_kw = {"FUNCTION","PARAM","IF","ELSEIF","ELSE","FOR","FOREACH","IN","WHILE","DO","UNTIL","SWITCH","RETURN","TRY","CATCH","FINALLY","THROW","TRAP","BREAK","CONTINUE","PROCESS","BEGIN","END"};
        static const std::unordered_set<std::string> bat_kw = {"ECHO","OFF","ON","SET","SETLOCAL","ENDLOCAL","IF","ELSE","GOTO","CALL","EXIT","FOR","IN","DO","PAUSE","CLS","SHIFT","START","NOT","EXIST","ERRORLEVEL"};
        static const std::unordered_set<std::string> pas_kw = {"PROGRAM","UNIT","INTERFACE","IMPLEMENTATION","BEGIN","END","VAR","TYPE","CONST","PROCEDURE","FUNCTION","IF","THEN","ELSE","WHILE","DO","FOR","TO","DOWNTO","REPEAT","UNTIL","ARRAY","OF","RECORD","INTEGER","STRING","BOOLEAN","REAL","CHAR"};
        static const std::unordered_set<std::string> bas_kw = {"REM","PRINT","INPUT","LET","IF","THEN","ELSE","ELSEIF","END","ENDIF","FOR","TO","STEP","NEXT","GOTO","GOSUB","RETURN","DIM","AS","SUB","FUNCTION","WHILE","WEND"};
        static const std::unordered_set<std::string> f_kw = {"PROGRAM","SUBROUTINE","FUNCTION","END","INTEGER","REAL","COMPLEX","LOGICAL","CHARACTER","DIMENSION","DO","WHILE","IF","THEN","ELSE","ELSEIF","ENDIF","CALL","RETURN","IMPLICIT","NONE","USE","MODULE","PRINT","WRITE","READ"};

        while (i < n) {
            if (i == 0 && (line[0] == '#' || line[0] == '@' || (line.rfind("$!", 0) == 0))) {
                if (lang == Lang::C_Cpp || lang == Lang::Batch) {
                    std::fill(tokens.begin(), tokens.end(), SyntaxTokenType::Directive);
                    break;
                }
            }

            if (SupportsSlashBlockComments(lang) && line[i] == '/' && i + 1 < n && line[i + 1] == '*') {
                const size_t close = line.find("*/", i + 2);
                const size_t covered = close == std::string::npos ? n : (std::min)(n, close + 2);
                std::fill(tokens.begin() + i, tokens.begin() + covered, SyntaxTokenType::Comment);
                if (close == std::string::npos || close + 2 > n) {
                    state.commentTerminator = '/';
                    return finish();
                }
                i = covered;
                continue;
            }

            if (lang == Lang::Pascal && line[i] == '{') {
                const size_t close = line.find('}', i + 1);
                const size_t covered = close == std::string::npos ? n : (std::min)(n, close + 1);
                std::fill(tokens.begin() + i, tokens.begin() + covered, SyntaxTokenType::Comment);
                if (close == std::string::npos || close + 1 > n) {
                    state.commentTerminator = '}';
                    return finish();
                }
                i = covered;
                continue;
            }

            if ((line[i] == '/' && i + 1 < n && line[i + 1] == '/') ||
                (line[i] == '#' && lang != Lang::C_Cpp) ||
                (line[i] == '!' && (lang == Lang::Fortran || lang == Lang::Basic)) ||
                (line[i] == '\'' && lang == Lang::Basic)) {
                std::fill(tokens.begin() + i, tokens.end(), SyntaxTokenType::Comment);
                break;
            }

            if (line[i] == '"' || line[i] == '\'' || line[i] == '`') {
                char quote = line[i];
                tokens[i] = SyntaxTokenType::String;
                size_t j = i + 1;
                while (j < n && line[j] != quote) {
                    if (line[j] == '\\' && j + 1 < n) tokens[j++] = SyntaxTokenType::String;
                    if (j < n) tokens[j++] = SyntaxTokenType::String;
                }
                if (j < n) tokens[j++] = SyntaxTokenType::String;
                else if (quote == '`' || (!line.empty() && line.back() == '\\')) state.quote = quote;
                i = j;
                continue;
            }

            if (std::isdigit((unsigned char)line[i])) {
                while (i < n && (std::isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == 'x')) {
                    tokens[i++] = SyntaxTokenType::Number;
                }
                continue;
            }

            if (std::isalpha((unsigned char)line[i]) || line[i] == '_' || line[i] == '$') {
                size_t start = i;
                while (i < n && (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '$' || line[i] == '-')) i++;
                std::string ident = line.substr(start, i - start);
                std::string uIdent = ToUpper(ident);

                SyntaxTokenType tt = SyntaxTokenType::Default;
                if (lang == Lang::C_Cpp || lang == Lang::Java || lang == Lang::Swift || lang == Lang::Kotlin) {
                    if (c_kw.count(ident)) tt = SyntaxTokenType::Keyword;
                    else if (c_ty.count(ident) || (!ident.empty() && std::isupper((unsigned char)ident[0]))) tt = SyntaxTokenType::Type;
                } else if (lang == Lang::Python && py_kw.count(ident)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::JavaScript && js_kw.count(ident)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::Shell && sh_kw.count(ident)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::PowerShell && ps_kw.count(uIdent)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::Batch && bat_kw.count(uIdent)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::Pascal && pas_kw.count(uIdent)) tt = SyntaxTokenType::Keyword;
                else if (lang == Lang::Basic && bas_kw.count(uIdent)) tt = SyntaxTokenType::Keyword;
                else if ((lang == Lang::Fortran || lang == Lang::Cobol) && f_kw.count(uIdent)) tt = SyntaxTokenType::Keyword;

                if (tt != SyntaxTokenType::Default) std::fill(tokens.begin() + start, tokens.begin() + i, tt);
                continue;
            }
            i++;
        }
        return finish();
    }
};

// Shared immutable line storage: undo/redo snapshots become O(1) shared_ptr vector
// copies instead of duplicating every line's character data on every keystroke.
using LinePtr = std::shared_ptr<const std::string>;
using BufferType = std::vector<LinePtr>;

class EveEditor {
private:
    struct EditSnapshot {
        BufferType buffer;
        int cursor_x = 0;
        int cursor_y = 0;
        bool modified = false;
    };

    struct BufferState {
        BufferType buffer;
        std::string filename;
        std::string buffer_name;
        Lang current_lang = Lang::Plain;
        std::vector<std::vector<SyntaxTokenType>> highlight_tokens;
        std::vector<HighlightState> highlight_end_states;
        std::vector<bool> highlight_valid;
        unsigned long long buffer_version = 0;
        int cursor_x = 0;
        int cursor_y = 0;
        int scroll_x = 0;
        int scroll_y = 0;
        bool modified = false;
        bool insert_mode = true;
        bool forward_dir = true;
        bool read_only = false;
        bool use_crlf = true;
        bool utf8_bom = false;
        std::vector<EditSnapshot> undo_history;
        std::vector<EditSnapshot> redo_history;
        ULONGLONG last_undo_snapshot_tick = 0;
        std::unordered_map<std::string, int> bookmarks;
        std::filesystem::file_time_type disk_write_time{};
        bool disk_write_time_valid = false;
        bool external_change_pending = false;
        int save_version = 1; // ODS-5 style generation counter, incremented on each successful save
        unsigned long long journaled_version = 0; // buffer_version last flushed to the recovery journal
    };
    std::deque<BufferState> buffers;
    size_t active_buffer_index = 0;
    BufferState* active_buf = nullptr;

    // Copy-on-write line access: reading a line never allocates beyond the returned copy,
    // and writing a line only replaces that one shared_ptr slot (unrelated undo/redo
    // snapshots and other lines keep sharing their existing string storage).
    static std::string GetLine(const BufferState& b, int y) {
        return (y >= 0 && y < (int)b.buffer.size() && b.buffer[y]) ? *b.buffer[y] : std::string();
    }
    static void SetLine(BufferState& b, int y, std::string value) {
        if (y >= 0 && y < (int)b.buffer.size()) b.buffer[y] = std::make_shared<const std::string>(std::move(value));
    }
    static void PushLine(BufferType& buf, std::string value) {
        buf.push_back(std::make_shared<const std::string>(std::move(value)));
    }
    static BufferType::iterator InsertLineAt(BufferType& buf, BufferType::iterator pos, std::string value) {
        return buf.insert(pos, std::make_shared<const std::string>(std::move(value)));
    }

    static std::string GetBufferNameFromPath(const std::string& path) {
        size_t last_slash = path.find_last_of("\\/");
        std::string name = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
        for (char& c : name) c = (char)std::toupper((unsigned char)c);
        return name;
    }

    // Builds a collision-resistant recovery/journal file stem from the buffer name and its
    // absolute path, so two different files that share a base name (e.g. notes.txt in two
    // different folders) never overwrite each other's crash-recovery data.
    static std::string UniqueRecoveryStem(const BufferState& buffer) {
        std::error_code error;
        const std::filesystem::path absolute = std::filesystem::absolute(buffer.filename, error);
        std::string key = error ? buffer.filename : absolute.string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        char hashHex[17];
        std::snprintf(hashHex, sizeof(hashHex), "%016zx", std::hash<std::string>{}(key));
        std::string safeName;
        safeName.reserve(buffer.buffer_name.size());
        for (char c : buffer.buffer_name) {
            const bool invalid = c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
            safeName.push_back(invalid ? '_' : c);
        }
        return safeName + "." + hashHex;
    }

    static bool LoadBufferFromFile(BufferState& target, const std::string& path) {
        target.buffer.clear();
        target.use_crlf = true;
        target.utf8_bom = false;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            PushLine(target.buffer, "");
            return false;
        }
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        target.utf8_bom = contents.size() >= 3 &&
            static_cast<unsigned char>(contents[0]) == 0xEF &&
            static_cast<unsigned char>(contents[1]) == 0xBB &&
            static_cast<unsigned char>(contents[2]) == 0xBF;
        const size_t start = target.utf8_bom ? 3 : 0;
        target.use_crlf = contents.find("\r\n", start) != std::string::npos;
        std::string line;
        for (size_t index = start; index < contents.size(); ++index) {
            if (contents[index] == '\n') {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                PushLine(target.buffer, std::move(line));
                line.clear();
            } else {
                line.push_back(contents[index]);
            }
        }
        if (!line.empty() || contents.empty() || contents.back() != '\n') PushLine(target.buffer, std::move(line));
        if (target.buffer.empty()) PushLine(target.buffer, "");
        std::error_code error;
        target.disk_write_time = std::filesystem::last_write_time(path, error);
        target.disk_write_time_valid = !error;
        target.external_change_pending = false;
        return true;
    }

    void LoadPreferences() {
        char homeDir[MAX_PATH] = {};
        const DWORD length = GetEnvironmentVariableA("USERPROFILE", homeDir, MAX_PATH);
        preferences_path = length > 0 && length < MAX_PATH ? std::filesystem::path(homeDir) / "eve.ini" : ".eve.ini";
        std::ifstream file(preferences_path);
        std::string line;
        while (std::getline(file, line)) {
            const size_t equals = line.find('=');
            if (equals == std::string::npos) continue;
            const std::string key = line.substr(0, equals), value = line.substr(equals + 1);
            if (key == "tab_width") { try { tab_width = (std::max)(1, (std::min)(16, std::stoi(value))); } catch (...) {} }
            else if (key == "line_numbers") show_line_numbers = value == "1";
            else if (key == "highlight") syntax_highlighting = value != "0";
            else if (key == "wrap") word_wrap = value == "1";
            else if (key == "case") search_case_sensitive = value != "0";
            else if (key == "word") search_whole_word = value == "1";
            else if (key == "theme") {
                static const std::unordered_map<std::string, EveTheme> themes = {
                    {"DEFAULT", EveTheme::Default}, {"BLACK", EveTheme::Black}, {"ORANGE", EveTheme::Orange},
                    {"GREEN", EveTheme::Green}, {"RED", EveTheme::Red}, {"BLUE", EveTheme::Blue}, {"PINK", EveTheme::Pink}
                };
                auto found = themes.find(value);
                if (found != themes.end()) current_theme = found->second;
            }
        }
    }

    void SavePreferences() const {
        if (preferences_path.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(preferences_path.parent_path(), error);
        std::ofstream file(preferences_path, std::ios::trunc);
        if (!file) return;
        file << "tab_width=" << tab_width << "\nline_numbers=" << show_line_numbers << "\nhighlight=" << syntax_highlighting
             << "\nwrap=" << word_wrap << "\ncase=" << search_case_sensitive << "\nword=" << search_whole_word
             << "\ntheme=" << ThemeName(current_theme) << "\n";
    }

    void CheckExternalChanges() {
        if (active_buf->filename.empty() || active_buf->filename == "NONAME.TXT") return;
        std::error_code error;
        const auto current = std::filesystem::last_write_time(active_buf->filename, error);
        if (!error && active_buf->disk_write_time_valid && current != active_buf->disk_write_time) {
            active_buf->external_change_pending = true;
            message = "%EVE-W-CHANGED, File changed on disk; use RELOAD to discard buffer changes";
        }
    }

    void WriteRecoveryFiles() const {
        std::error_code error;
        const std::filesystem::path recovery = std::filesystem::temp_directory_path(error) / "eve-recovery";
        if (error) return;
        std::filesystem::create_directories(recovery, error);
        for (const auto& buffer : buffers) {
            if (!buffer.modified) continue;
            const std::filesystem::path output = recovery / (UniqueRecoveryStem(buffer) + ".recovery.txt");
            std::ofstream file(output, std::ios::binary | std::ios::trunc);
            for (const auto& line : buffer.buffer) file << *line << (buffer.use_crlf ? "\r\n" : "\n");
        }
    }

    void LoadActiveBufferState(size_t new_index) {
        if (new_index >= buffers.size()) return;
        active_buffer_index = new_index;
        active_buf = &buffers[active_buffer_index];
        rendered_valid = false;
        needs_render = true;
    }

    
    
    
    std::string message = "EVE 7.6.1  Press Ctrl+Z for Command prompt, F1 for GOLD key. Type HELP for manual.";
    std::string command_input = "";
    std::string clipboard;
    std::vector<std::string> command_history;
    int command_history_index = -1;
    std::string frame_buffer;
    
    
    
    

    
    
    
    
    int help_scroll_y = 0;
    int pending_help_scroll_lines = 0;

    // Snapshot of the last painted frame, used to redraw scrolls incrementally.
    
    bool rendered_valid = false;
    unsigned long long rendered_buffer_version = 0;
    int rendered_scroll_x = 0;
    int rendered_scroll_y = 0;
    int rendered_width = 0;
    int rendered_height = 0;
    bool rendered_selection = false;
    std::string rendered_status;
    std::string rendered_footer;

    
    
    
    bool in_command_mode = false;
    
    bool in_help_mode = false;
    bool gold_armed = false;
    bool running = true;
    bool needs_render = true;
    bool selecting = false;
    int sel_start_x = 0, sel_start_y = 0;
    std::string last_search;
    bool search_case_sensitive = true;
    bool search_whole_word = false;
    bool syntax_highlighting = true;
    bool show_line_numbers = false;
    bool word_wrap = false;
    int tab_width = 4;
    EveTheme current_theme = EveTheme::Default;
    bool rectangular_selection = false;
    bool preferences_dirty = false;
    std::filesystem::path preferences_path;
    std::string pending_recovery_path;

    // GOLD (F1/PF1) key state and the GOLD-K/GOLD-E learn-execute macro buffer.
    wchar_t pending_high_surrogate = 0;
    bool learning = false;
    bool replaying_learn = false;
    std::vector<KEY_EVENT_RECORD> learn_buffer;
    bool learn_has_content = false;

    // Two-window (split screen) support: the secondary pane views the same buffer at other_scroll_y.
    bool split_view = false;
    int other_scroll_y = 0;

    // FILL / autoindent formatting state.
    int left_margin = 0;
    int right_margin = 79;
    bool auto_indent = false;

    // Per-buffer EXIT confirmation ("Save changes to <buffer>? Y/N").
    bool in_exit_confirm = false;
    std::vector<size_t> exit_confirm_pending;
    size_t exit_confirm_cursor = 0;

    // Background journal writer: decouples journal disk I/O from the input loop.
    std::thread journal_writer_thread;
    std::mutex journal_writer_mutex;
    std::condition_variable journal_writer_cv;
    std::atomic<bool> journal_writer_running{false};
    bool journal_writer_stop = false;
    std::map<std::filesystem::path, std::string> journal_writer_queue;

    HANDLE hIn, hOut;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD original_input_mode = 0;
    DWORD original_output_mode = 0;
    bool console_cleaned_up = false;

    static bool IsUtf8ContinuationByte(unsigned char value) {
        return (value & 0xC0) == 0x80;
    }

    static int Utf8Previous(const std::string& text, int offset) {
        if (offset <= 0) return 0;
        int previous = offset - 1;
        while (previous > 0 && IsUtf8ContinuationByte(static_cast<unsigned char>(text[previous]))) {
            --previous;
        }
        return previous;
    }

    static int Utf8Next(const std::string& text, int offset) {
        if (offset >= static_cast<int>(text.size())) return static_cast<int>(text.size());
        int next = offset + 1;
        while (next < static_cast<int>(text.size()) && IsUtf8ContinuationByte(static_cast<unsigned char>(text[next]))) {
            ++next;
        }
        return next;
    }

    static uint32_t Utf8CodePoint(const std::string& text, int offset) {
        const unsigned char first = static_cast<unsigned char>(text[offset]);
        if (first < 0x80) return first;
        const int next = Utf8Next(text, offset);
        uint32_t codePoint = first & ((1u << (7 - (next - offset))) - 1u);
        for (int index = offset + 1; index < next; ++index) {
            codePoint = (codePoint << 6) | (static_cast<unsigned char>(text[index]) & 0x3Fu);
        }
        return codePoint;
    }

    static int Utf8DisplayWidth(const std::string& text, int offset) {
        const uint32_t codePoint = Utf8CodePoint(text, offset);
        if ((codePoint >= 0x0300 && codePoint <= 0x036F) ||
            (codePoint >= 0x1AB0 && codePoint <= 0x1AFF) ||
            (codePoint >= 0x1DC0 && codePoint <= 0x1DFF) ||
            (codePoint >= 0x20D0 && codePoint <= 0x20FF) ||
            (codePoint >= 0xFE20 && codePoint <= 0xFE2F)) {
            return 0;
        }
        if ((codePoint >= 0x1100 && codePoint <= 0x115F) ||
            (codePoint >= 0x2E80 && codePoint <= 0xA4CF) ||
            (codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||
            (codePoint >= 0xF900 && codePoint <= 0xFAFF) ||
            (codePoint >= 0xFE10 && codePoint <= 0xFE19) ||
            (codePoint >= 0xFE30 && codePoint <= 0xFE6F) ||
            (codePoint >= 0xFF00 && codePoint <= 0xFF60) ||
            (codePoint >= 0xFFE0 && codePoint <= 0xFFE6) ||
            (codePoint >= 0x1F300 && codePoint <= 0x1FAFF)) {
            return 2;
        }
        return 1;
    }

    void SaveUndoSnapshot() {
        const ULONGLONG now = GetTickCount64();
        if (active_buf->last_undo_snapshot_tick != 0 && now - active_buf->last_undo_snapshot_tick < 500) return;
        EditSnapshot snapshot;
        snapshot.buffer = active_buf->buffer;
        snapshot.cursor_x = active_buf->cursor_x;
        snapshot.cursor_y = active_buf->cursor_y;
        snapshot.modified = active_buf->modified;
        active_buf->undo_history.push_back(std::move(snapshot));
        if (active_buf->undo_history.size() > 100) active_buf->undo_history.erase(active_buf->undo_history.begin());
        active_buf->redo_history.clear();
        active_buf->last_undo_snapshot_tick = now;
    }

    void RestoreSnapshot(EditSnapshot&& snapshot, std::vector<EditSnapshot>& destination, const char* action) {
        EditSnapshot current;
        current.buffer = std::move(active_buf->buffer);
        current.cursor_x = active_buf->cursor_x;
        current.cursor_y = active_buf->cursor_y;
        current.modified = active_buf->modified;
        destination.push_back(std::move(current));
        active_buf->buffer = std::move(snapshot.buffer);
        active_buf->cursor_x = snapshot.cursor_x;
        active_buf->cursor_y = snapshot.cursor_y;
        active_buf->modified = snapshot.modified;
        selecting = false;
        SyncHighlightStorage();
        InvalidateHighlightFrom(0);
        message = action;
    }

    void Undo() {
        if (active_buf->undo_history.empty()) { message = "Nothing to undo"; return; }
        EditSnapshot snapshot = std::move(active_buf->undo_history.back());
        active_buf->undo_history.pop_back();
        RestoreSnapshot(std::move(snapshot), active_buf->redo_history, "Undo");
    }

    void Redo() {
        if (active_buf->redo_history.empty()) { message = "Nothing to redo"; return; }
        EditSnapshot snapshot = std::move(active_buf->redo_history.back());
        active_buf->redo_history.pop_back();
        RestoreSnapshot(std::move(snapshot), active_buf->undo_history, "Redo");
    }

    void SyncHighlightStorage() {
        const size_t lineCount = active_buf->buffer.size();
        active_buf->highlight_tokens.resize(lineCount);
        active_buf->highlight_end_states.resize(lineCount);
        active_buf->highlight_valid.resize(lineCount, false);
    }

    void InvalidateHighlightLine(int line) {
        if (line < 0 || line >= static_cast<int>(active_buf->buffer.size())) return;
        SyncHighlightStorage();
        active_buf->highlight_valid[line] = false;
        ++active_buf->buffer_version;
    }

    void InvalidateHighlightFrom(int firstLine) {
        ++active_buf->buffer_version;
        SyncHighlightStorage();
        const size_t lineCount = active_buf->buffer.size();
        for (size_t line = static_cast<size_t>((std::max)(0, firstLine)); line < lineCount; ++line) {
            active_buf->highlight_valid[line] = false;
        }
    }

    void EnsureHighlightThrough(int lastLine) {
        if (active_buf->buffer.empty() || lastLine < 0) return;

        lastLine = (std::min)(lastLine, static_cast<int>(active_buf->buffer.size()) - 1);
        SyncHighlightStorage();

        int firstInvalid = 0;
        while (firstInvalid <= lastLine && active_buf->highlight_valid[firstInvalid]) ++firstInvalid;
        if (firstInvalid > lastLine) return;

        HighlightState state{};
        if (firstInvalid > 0) state = active_buf->highlight_end_states[firstInvalid - 1];
        for (int line = firstInvalid; line <= lastLine; ++line) {
            HighlightState nextEndState{};
            active_buf->highlight_tokens[line] = Highlighter::HighlightLine(
                *active_buf->buffer[line], active_buf->current_lang, state, &nextEndState);
            const bool stateUnchanged = active_buf->highlight_valid[line] && (nextEndState == active_buf->highlight_end_states[line]);
            active_buf->highlight_end_states[line] = nextEndState;
            active_buf->highlight_valid[line] = true;
            state = nextEndState;
            if (stateUnchanged && line + 1 <= lastLine && active_buf->highlight_valid[line + 1]) {
                break;
            }
        }
    }

    int VisualColumn(int lineIndex, int byteOffset) const {
        if (lineIndex < 0 || lineIndex >= static_cast<int>(active_buf->buffer.size())) return 0;
        const std::string& line = *active_buf->buffer[lineIndex];
        const int limit = (std::min)(byteOffset, static_cast<int>(line.size()));
        int column = 0;
        for (int offset = 0; offset < limit;) {
            if (line[offset] == '\t') {
                column += tab_width - (column % tab_width);
                ++offset;
            } else {
                column += Utf8DisplayWidth(line, offset);
                offset = Utf8Next(line, offset);
            }
        }
        return column;
    }

    int VisualToByteIndex(const std::string& line, int visual_col) const {
        int char_idx = 0;
        int visual_x = 0;
        int len = (int)line.size();
        while (char_idx < len && visual_x < visual_col) {
            if (line[char_idx] == '\t') {
                int tabWidth = tab_width - (visual_x % tab_width);
                if (visual_x + tabWidth > visual_col) break;
                visual_x += tabWidth;
                char_idx++;
            } else {
                int width = Utf8DisplayWidth(line, char_idx);
                if (visual_x + width > visual_col) break; // Don't overshoot
                visual_x += width;
                char_idx = Utf8Next(line, char_idx);
            }
        }
        return char_idx;
    }

    std::string ToUpper(std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }

    std::string Trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

public:
    EveEditor(const std::vector<std::string>& files, bool ro = false) {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        // Enable Mouse Input and disable QuickEdit
        DWORD mode = 0;
        GetConsoleMode(hIn, &mode);
        original_input_mode = mode;
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
        mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        SetConsoleMode(hIn, mode);

        DWORD outMode = 0;
        GetConsoleMode(hOut, &outMode);
        original_output_mode = outMode;
        SetConsoleMode(hOut, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
        LoadPreferences();

        if (!files.empty()) {
            for (const auto& file : files) {
                BufferState b;
                b.filename = file;
                b.buffer_name = GetBufferNameFromPath(file);
                b.read_only = ro;

                LoadBufferFromFile(b, file);
                b.current_lang = Highlighter::DetectLanguage(file);

                DWORD attrib = GetFileAttributesA(file.c_str());
                if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_READONLY)) {
                    b.read_only = true;
                }

                b.highlight_tokens.resize(b.buffer.size());
                b.highlight_end_states.resize(b.buffer.size(), HighlightState{});
                b.highlight_valid.resize(b.buffer.size(), false);

                buffers.push_back(std::move(b));
            }
        }

        if (buffers.empty()) {
            BufferState b;
            PushLine(b.buffer, "");
            b.filename = "NONAME.TXT";
            b.buffer_name = "MAIN";
            b.read_only = ro;
            b.highlight_tokens.resize(1);
            b.highlight_end_states.resize(1, HighlightState{});
            b.highlight_valid.resize(1, false);
            buffers.push_back(std::move(b));
        }

        LoadActiveBufferState(0);
        InvalidateHighlightFrom(0);
        message = "EVE 7.6.1  Press Ctrl+PageUp/PageDown to switch buffers. Press Ctrl+Z for commands.";

        for (const auto& buffer : buffers) {
            std::error_code error;
            if (std::filesystem::exists(JournalPathFor(buffer), error) && !error) {
                message = "%EVE-I-JOURNAL, Journal file found for " + buffer.buffer_name + "; type RECOVER to restore";
                break;
            }
        }

        StartJournalWriterThread();
    }

    ~EveEditor() {
        StopJournalWriterThread();
        if (g_consoleCtrlRequested.load(std::memory_order_relaxed)) WriteRecoveryFiles();
        SavePreferences();
        SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
        CleanupConsole();
    }

    // Journal writes are handed off to a background thread so a slow disk/network
    // share never stalls the interactive input loop; only the already-serialized
    // content string crosses threads, never the live buffer itself.
    void StartJournalWriterThread() {
        if (journal_writer_running.exchange(true)) return;
        journal_writer_thread = std::thread([this]() {
            while (true) {
                std::unique_lock<std::mutex> lock(journal_writer_mutex);
                journal_writer_cv.wait(lock, [this]() { return !journal_writer_queue.empty() || journal_writer_stop; });
                if (journal_writer_queue.empty()) {
                    if (journal_writer_stop) return;
                    continue;
                }
                auto it = journal_writer_queue.begin();
                std::filesystem::path path = it->first;
                std::string content = std::move(it->second);
                journal_writer_queue.erase(it);
                lock.unlock();
                std::ofstream file(path, std::ios::binary | std::ios::trunc);
                if (file) file << content;
            }
        });
    }

    void StopJournalWriterThread() {
        if (!journal_writer_running.exchange(false)) return;
        {
            std::lock_guard<std::mutex> lock(journal_writer_mutex);
            journal_writer_stop = true;
        }
        journal_writer_cv.notify_all();
        if (journal_writer_thread.joinable()) journal_writer_thread.join();
    }

    void LoadFile(const std::string& path) {
        if (LoadBufferFromFile(*active_buf, path)) {
            message = std::to_string(active_buf->buffer.size()) + " lines read from file " + path;
        } else {
            message = "[New File: " + path + "]";
        }
        active_buf->current_lang = Highlighter::DetectLanguage(path);
        InvalidateHighlightFrom(0);
        active_buf->modified = false;
        active_buf->cursor_x = active_buf->cursor_y = 0;

        DWORD attrib = GetFileAttributesA(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_READONLY)) {
            active_buf->read_only = true;
        } else if (attrib != INVALID_FILE_ATTRIBUTES) {
            active_buf->read_only = (attrib & FILE_ATTRIBUTE_READONLY) != 0;
        }
    }

    bool SaveFile(const std::string& path) {
        if (active_buf->read_only && path == active_buf->filename) {
            std::cout << "\a";
            std::cout.flush();
            message = "%EVE-E-READONLY, Buffer is read-only; modifications not allowed";
            return false;
        }
        const std::string temporaryPath = path + ".eve.tmp";
        std::ofstream f(temporaryPath, std::ios::binary | std::ios::trunc);
        if (f.is_open()) {
            if (active_buf->utf8_bom) f << "\xEF\xBB\xBF";
            const char* newline = active_buf->use_crlf ? "\r\n" : "\n";
            for (const auto& line : active_buf->buffer) f << *line << newline;
            f.close();
            if (!f) {
                std::remove(temporaryPath.c_str());
                message = "%EVE-E-WRITE, error writing " + path;
                return false;
            }
            const std::string backupPath = path + ".bak";
            std::remove(backupPath.c_str());
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES && !MoveFileExA(path.c_str(), backupPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                std::remove(temporaryPath.c_str());
                message = "%EVE-E-BACKUP, error backing up " + path;
                return false;
            }
            if (!MoveFileExA(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                MoveFileExA(backupPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
                std::remove(temporaryPath.c_str());
                message = "%EVE-E-RENAME, error replacing " + path;
                return false;
            }
            active_buf->modified = false;
            ++active_buf->save_version;
            std::error_code error;
            active_buf->disk_write_time = std::filesystem::last_write_time(path, error);
            active_buf->disk_write_time_valid = !error;
            active_buf->external_change_pending = false;
            active_buf->filename = path;
            active_buf->buffer_name = GetBufferNameFromPath(path);
            active_buf->current_lang = Highlighter::DetectLanguage(path);
            std::remove(JournalPathFor(*active_buf).string().c_str());
            message = std::to_string(active_buf->buffer.size()) + " lines written to " + path;
            return true;
        } else {
            message = "%EVE-E-OPENOUT, error creating " + path;
            return false;
        }
    }

    void GetWindowSize(int& width, int& height) {
        GetConsoleScreenBufferInfo(hOut, &csbi);
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    const std::vector<std::string>& HelpLines() const {
        static const std::vector<std::string> lines = {
            "================================================================================",
            "                    EVE (Extensible Versatile Editor) HELP                     ",
            "================================================================================",
            "",
            " GETTING STARTED:",
            "   EVE is a keyboard-friendly console text editor inspired by VMS EVE.",
            "   Edit normally in the main screen. Press Ctrl+Z or ESC to open the",
            "   Command: prompt, type a command, and press Enter. Type HELP anytime",
            "   at the Command: prompt to return to this manual.",
            "",
            " THE GOLD KEY:",
            "   F1 emulates the LK201 keyboard's GOLD (PF1) key from real VMS terminals.",
            "   Press F1 once to arm GOLD, then press a command key; GOLD applies to",
            "   only the next keystroke. Press F1 again to cancel without acting.",
            "",
            "   GOLD sequences:",
            "     GOLD then T                   TOP - jump to start of buffer",
            "     GOLD then B                   BOTTOM - jump to end of buffer",
            "     GOLD then F                   FINDNEXT - repeat the last search",
            "     GOLD then L                   Open Command: prompt pre-filled with GOTO",
            "     GOLD then U                   UNDO the last edit",
            "     GOLD then Y                   REDO the last undone edit",
            "     GOLD then W                   WRITE/SAVE the active buffer",
            "     GOLD then R                   Cut (REMOVE) selection or current line",
            "     GOLD then P                   PASTE clipboard at the cursor",
            "     GOLD then D                   Remove one character in the current direction",
            "     GOLD then J                   FILL - reformat the paragraph at the cursor",
            "     GOLD then K                   Start/stop recording a LEARN sequence",
            "     GOLD then E                   EXECUTE the recorded LEARN sequence",
            "     GOLD then 2                   Toggle the split (two-window) view",
            "     GOLD then 9 / GOLD then 0     Scroll the other window up / down",
            "     GOLD then .                   SELECT - drop a selection anchor",
            "     GOLD then A                   Toggle Insert / Overstrike mode",
            "     GOLD then C                   Toggle Forward / Reverse search direction",
            "     GOLD then H                   Show this HELP screen",
            "",
            " LEARN AND EXECUTE (KEYSTROKE MACROS):",
            "   LEARN records every editing keystroke until you stop it; EXECUTE replays",
            "   the recorded sequence, so repetitive edits only need to be typed once.",
            "",
            "   Commands:",
            "     LEARN                         Start recording; run again to stop",
            "     EXECUTE                       Replay the most recently recorded sequence",
            "",
            " FILL, MARGINS, AND AUTO INDENT:",
            "   FILL rewraps the paragraph under the cursor between the left and right",
            "   margins. Auto indent copies the previous line's leading whitespace onto",
            "   each new line, and the left margin also pads new lines to that column.",
            "",
            "   Commands:",
            "     FILL                           Reformat the current paragraph",
            "     SET LEFT MARGIN <n>            Set the left margin column",
            "     SET RIGHT MARGIN <n>           Set the right margin (fill width)",
            "     SET AUTO INDENT ON|OFF         Toggle auto indent on new lines",
            "",
            " TWO WINDOWS (SPLIT VIEW):",
            "   SPLIT (or SET SPLIT ON) divides the screen so a second window shows a",
            "   different scroll position in the same buffer, matching EVE's classic",
            "   two-window editing. Use GOLD-9/GOLD-0 to move the other window.",
            "",
            "   Commands:",
            "     SPLIT / SET SPLIT ON|OFF       Toggle the second window",
            "",
            " CRASH RECOVERY AND JOURNALING:",
            "   EVE continuously journals the active buffer's content to a recovery file",
            "   as you type. If EVE starts and finds a journal for an opened file, it",
            "   reports this so you can decide whether to restore it.",
            "",
            "   Commands:",
            "     RECOVER                        Replace the buffer with its journal contents",
            "",
            " COLOR THEMES:",
            "   THEME sets the status bar and window divider color. The color name is",
            "   given as a separate word after the command.",
            "",
            "   Commands:",
            "     THEME default|black|orange|green|red|blue|pink   Set the color theme",
            "",
            "   Examples:",
            "     THEME blue                    Use the blue status bar theme",
            "     THEME default                 Return to the terminal's own colors",
            "",
            " OPENING FILES:",
            "   From the command line:",
            "     eve notes.txt                 Open notes.txt when EVE starts",
            "     eve src\\main.cpp             Open a file in another directory",
            "",
            "   From inside EVE:",
            "     1. Press Ctrl+Z or ESC to show Command:",
            "     2. Type OPEN filename or GET filename",
            "     3. Press Enter",
            "",
            "   Examples:",
            "     OPEN notes.txt                Open notes.txt",
            "     GET todo.md                   Open todo.md",
            "     INCLUDE src\\config.h         INCLUDE is accepted as an OPEN alias",
            "",
            "   If the named file is already open, EVE switches to that buffer instead",
            "   of loading a duplicate. If the file does not exist yet, EVE creates a",
            "   new empty buffer with that filename so you can start writing immediately.",
            "",
            " SAVING FILES:",
            "   Quick save the current file:",
            "     Ctrl+W                        Save the active buffer to its filename",
            "",
            "   Save from the Command: prompt:",
            "     SAVE                          Save using the current filename",
            "     WRITE                         Same as SAVE",
            "",
            "   Examples:",
            "     SAVE                          Write changes to the current file",
            "     WRITE                         Same result as SAVE",
            "",
            " SAVE AS / WRITING TO A NEW NAME:",
            "   Use SAVE followed by a filename to write the current buffer to a new path.",
            "   This is EVE's Save As workflow.",
            "",
            "   Examples:",
            "     SAVE draft.txt                Save current buffer as draft.txt",
            "     SAVE backup\\notes-copy.txt    Save a copy in the backup directory",
            "     WRITE report-final.md         WRITE also accepts a filename",
            "",
            "   A successful Save As becomes the buffer's new filename for Ctrl+W.",
            "   EVE preserves BOM and CRLF/LF style, writes through a temporary file,",
            "   and retains an existing destination as filename.bak before replacement.",
            "",
            " BUFFERS AND MULTIPLE FILES:",
            "   EVE can keep several files open at once. Each open file is a buffer.",
            "",
            "   Keys:",
            "     Ctrl+Page Up / Ctrl+Page Down Cycle between open buffers",
            "",
            "   Commands:",
            "     SHOW BUFFERS                  List all open buffers",
            "     BUFFERS                       Same as SHOW BUFFERS",
            "     BUFFER <name>                 Switch to a buffer by name",
            "     CLOSE [/FORCE]                Close current buffer; FORCE discards edits",
            "",
            "   Examples:",
            "     OPEN readme.md                Open readme.md in another buffer",
            "     OPEN src\\eve.cpp             Open source file in another buffer",
            "     SHOW BUFFERS                  See every open buffer",
            "     BUFFER readme.md              Switch back to readme.md",
            "",
            " MOVEMENT:",
            "   Arrow Keys                      Move cursor normally",
            "   Ctrl+Left / Ctrl+Right          Move by word",
            "   Ctrl+Up / Ctrl+Down             Move by paragraph",
            "   Home / End                      Jump to start / end of line",
            "   Page Up / Page Down             Scroll by screen page",
            "   TOP                             Jump to the start of the buffer",
            "   BOTTOM                          Jump to the end of the buffer",
            "   GOTO LINE <n>                   Jump to line number <n>",
            "",
            "   Examples:",
            "     GOTO LINE 1                   Go to the first line",
            "     GOTO LINE 120                 Go to line 120",
            "     TOP                           Return to the top quickly",
            "",
            " EDITING:",
            "   Type text normally at the cursor. Insert mode adds new text; overstrike",
            "   mode replaces existing text.",
            "",
            "   Keys:",
            "     Tab                           Insert a hard tab using four-column stops",
            "     Shift+Arrow/Home/End/Page     Extend the current text selection",
            "     Ctrl+Tab / Ctrl+Shift+Tab     Indent / outdent selected lines",
            "     Backspace                     Delete character before cursor",
            "     Delete                        Delete character under cursor",
            "     Ctrl+A                        Toggle Insert / Overstrike mode",
            "     Ctrl+Y                        Redo the last undone edit",
            "     Ctrl+F                        Open a FIND command",
            "     F3                            Repeat the last search",
            "     SET TAB WIDTH <1-16>          Set displayed tab-stop width",
            "",
            "   Commands:",
            "     SET INSERT                    Switch to insert mode",
            "     SET OVERSTRIKE                Switch to replace mode",
            "",
            " SELECTION, COPY, CUT, AND PASTE:",
            "   SELECT drops an anchor at the cursor. Move the cursor to extend the",
            "   selected range, then COPY or CUT it. Shift+navigation also extends a",
            "   selection. If nothing is selected, COPY and CUT operate on the current line.",
            "",
            "   Keys:",
            "     Ctrl+K                        Cut selected text or current line",
            "     Ctrl+U                        Paste Windows clipboard text",
            "",
            "   Commands:",
            "     SELECT                        Start selecting text",
            "     COPY                          Copy selection/current line to Windows clipboard",
            "     REMOVE                        Cut selection/current line to Windows clipboard",
            "     CUT                           Same as REMOVE",
            "     INSERT HERE                   Paste clipboard at cursor",
            "     PASTE                         Same as INSERT HERE",
            "",
            "   Examples:",
            "     SELECT                        Mark the start of a block",
            "     COPY                          Copy the selected block",
            "     INSERT HERE                   Paste it at the cursor",
            "     SET RECTANGLE ON              Select columns instead of text ranges",
            "",
            " SEARCHING:",
            "   FIND searches from the cursor in the current search direction.",
            "   REPLACE changes the next match by default. REPLACE ALL changes every",
            "   match in the current buffer. Use quotes when either value contains spaces.",
            "",
            "   Commands:",
            "     FIND <string>                 Search for text",
            "     FINDNEXT / NEXT               Repeat the latest search",
            "     REPLACE <old> <new>           Replace next match from the cursor",
            "     REPLACE ALL <old> <new>       Replace every match in current buffer",
            "     SUBSTITUTE <old> <new>        Same as REPLACE",
            "     SET FORWARD                   Search forward from the cursor",
            "     SET REVERSE                   Search backward from the cursor",
            "     SET CASE ON|OFF               Enable or disable case-sensitive search",
            "     SET WORD ON|OFF               Match whole words or substrings",
            "",
            "   Examples:",
            "     FIND main                     Search for main",
            "     REPLACE main WinMain          Replace the next main with WinMain",
            "     REPLACE ALL foo bar           Replace every foo with bar",
            "     REPLACE \"old text\" \"new text\" Replace the next phrase match",
            "     REPLACE ALL temp \"\"          Delete every temp by replacing with nothing",
            "     SET REVERSE                   Make the next FIND search backward",
            "     FIND TODO                     Search backward for TODO",
            "     F3                            Repeat the most recent search",
            "",
            " BOOKMARKS AND FILE CHANGES:",
            "   MARK <name>                     Save the current line as a bookmark",
            "   GOTO MARK <name>                Jump to a bookmark",
            "   SHOW MARKS                      List current-buffer bookmarks",
            "   RELOAD [/FORCE]                 Reload a file changed outside EVE",
            "   EVE warns when an open file changes on disk. On Ctrl+C or console close,",
            "   modified buffers are copied to the temporary eve-recovery folder.",
            "",
            " MOUSE CONTROLS:",
            "   Left Click                      Position cursor anywhere in buffer",
            "   Left Click + Drag               Select text range",
            "   Scroll Wheel Up / Down          Scroll this help or the text viewport",
            "",
            " EXITING:",
            "   EXIT / EX                       Save modified buffers (asks Y/N per buffer) and exit",
            "   QUIT / Q                        Exit when no edits are pending",
            "   QUIT / Q /FORCE                 Discard unsaved edits and exit",
            "",
            "   EXIT asks 'Save changes to <buffer>? (Y/N)' for each modified buffer in",
            "   turn; press Ctrl+C at the prompt to cancel EXIT without leaving.",
            "",
            "   Examples:",
            "     EXIT                          Review and save changes, then leave EVE",
            "     QUIT                          Leave without saving changes",
            "",
            " COMMON WORKFLOWS:",
            "   Open, edit, save, exit:",
            "     eve notes.txt",
            "     Type your changes",
            "     Press Ctrl+W",
            "     Press Ctrl+Z, type EXIT, press Enter",
            "",
            "   Open another file while editing:",
            "     Press Ctrl+Z",
            "     Type OPEN todo.md",
            "     Press Enter",
            "     Use Ctrl+Page Up / Ctrl+Page Down to switch buffers",
            "",
            "   Find and replace text:",
            "     Press Ctrl+Z",
            "     Type FIND draft",
            "     Press Enter",
            "     Press Ctrl+Z",
            "     Type REPLACE draft final",
            "     Press Enter",
            "",
            "   Save As:",
            "     Press Ctrl+Z",
            "     Type SAVE notes-backup.txt",
            "     Press Enter",
            "",
            " COMMAND PROMPT PRODUCTIVITY:",
            "   Up / Down                       Recall recent commands",
            "   Tab                             Complete command names and path arguments",
            "   Abbreviations                   Any unambiguous command prefix works, DCL-style",
            "                                   (e.g. EXI for EXIT, WR for WRITE, REPL for REPLACE)",
            "",
            " COMMAND SUMMARY:",
            "   HELP                            Display this help reference",
            "   OPEN <filename>                 Open file into new or existing buffer",
            "   GET <filename>                  Same as OPEN",
            "   INCLUDE <filename>              Same as OPEN",
            "   SAVE [filename]                 Save current buffer, optionally as filename",
            "   WRITE [filename]                Same as SAVE",
            "   SHOW BUFFERS / BUFFERS          List all open buffers",
            "   BUFFER <name>                   Switch to a buffer by name",
            "   CLOSE [/FORCE]                  Close current buffer",
            "   FIND <string>                   Search text",
            "   FINDNEXT / NEXT                 Repeat latest search",
            "   REPLACE <old> <new>             Replace next match from cursor",
            "   REPLACE ALL <old> <new>         Replace all matches in current buffer",
            "   SUBSTITUTE <old> <new>          Same as REPLACE",
            "   SET FORWARD / SET REVERSE       Change search direction",
            "   SET INSERT / SET OVERSTRIKE     Change edit mode",
            "   SET CASE ON|OFF                 Set search case sensitivity",
            "   SET WORD ON|OFF                 Set whole-word matching",
            "   SET LINE NUMBERS ON|OFF         Show or hide line numbers",
            "   SET HIGHLIGHT ON|OFF            Enable or disable syntax colors",
            "   SET WRAP ON|OFF                 Enable or disable wrap mode",
            "   SET LANGUAGE <name>             Set language highlighting",
            "   SET TAB WIDTH <1-16>            Set tab-stop width",
            "   SET RECTANGLE ON|OFF            Select columns or text ranges",
            "   TOP / BOTTOM                    Jump to start or end of buffer",
            "   GOTO LINE <n>                   Jump to line number <n>",
            "   MARK <name>                     Mark current line",
            "   GOTO MARK <name>                Jump to bookmark",
            "   SHOW MARKS                      List bookmarks",
            "   RELOAD [/FORCE]                 Reload current file from disk",
            "   SELECT                          Start text selection",
            "   COPY                            Copy selected text or current line",
            "   REMOVE / CUT                    Cut selected text or current line",
            "   INSERT HERE / PASTE             Paste clipboard at cursor",
            "   UNDO / REDO                     Undo or redo the latest edit",
            "   FILL                            Reformat the paragraph at the cursor",
            "   SET LEFT MARGIN <n>             Set the left margin column",
            "   SET RIGHT MARGIN <n>            Set the right margin (fill width)",
            "   SET AUTO INDENT ON|OFF          Toggle auto indent on new lines",
            "   LEARN                           Start/stop recording a keystroke macro",
            "   EXECUTE                         Replay the recorded keystroke macro",
            "   SPLIT / SET SPLIT ON|OFF        Toggle the two-window split view",
            "   RECOVER                         Restore the buffer from its journal file",
            "   THEME <color>                   Set color theme: default, black, orange,",
            "                                   green, red, blue, pink",
            "   EXIT / EX                       Save modified buffers (Y/N per buffer) and exit",
            "   QUIT / Q [/FORCE]               Exit; FORCE discards unsaved changes",
            "",
            " PREFERENCES: Tab width, search, line-number, highlight, and wrap settings",
            " are saved automatically in the user EVE configuration file.",
            "",
            " SUPPORTED SYNTAX HIGHLIGHTING:",
            "   C, C++, Java, Python, Swift, Kotlin, Fortran, COBOL, Pascal, JavaScript,",
            "   BASIC, PowerShell, CMD/BAT, Zsh, Bash, Fish.",
            "================================================================================",
            " Use the scroll wheel to read more. Press any key or Click to return to buffer..."
        };
        return lines;
    }

    void CleanupConsole() {
        if (console_cleaned_up) return;
        console_cleaned_up = true;

        std::cout << "\x1b[2J\x1b[3J\x1b[H\x1b[?25h";
        std::cout.flush();

        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            CONSOLE_SCREEN_BUFFER_INFO info{};
            if (GetConsoleScreenBufferInfo(hOut, &info)) {
                const COORD home{0, 0};
                const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
                DWORD written = 0;
                FillConsoleOutputCharacterA(hOut, ' ', cells, home, &written);
                FillConsoleOutputAttribute(hOut, info.wAttributes, cells, home, &written);
                SetConsoleCursorPosition(hOut, home);
            }
            if (original_output_mode != 0) SetConsoleMode(hOut, original_output_mode);
        }
        if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr && original_input_mode != 0) {
            SetConsoleMode(hIn, original_input_mode);
        }
    }

    void FormatHelpLine(int lineIndex, int width, std::string& out) const {
        const auto& lines = HelpLines();
        std::string line;

        if (lineIndex < static_cast<int>(lines.size())) {
            line = lines[lineIndex];
            out += "\x1b[96m" + line;
            if (static_cast<int>(line.size()) < width) {
                out.append(width - line.size(), ' ');
            }
        } else {
            out += "\x1b[96m";
            out.append(width, ' ');
        }
        out += "\x1b[0m";
    }

    void RenderHelp(int width, int height) {
        std::string h_screen;
        h_screen.reserve(16384);
        h_screen += "\x1b[?2026h\x1b[?25l\x1b[H";

        for (int r = 0; r < height; ++r) {
            FormatHelpLine(help_scroll_y + r, width, h_screen);
            h_screen += "\n";
        }
        h_screen += "\x1b[?2026l";
        std::cout << h_screen;
        std::cout.flush();
    }

    void RenderScrolledHelp(int width, int height, int scrollLines) {
        if (scrollLines <= -height || scrollLines >= height) {
            RenderHelp(width, height);
            return;
        }

        const int exposedLines = scrollLines < 0 ? -scrollLines : scrollLines;
        std::string frame;
        frame.reserve(16384);
        frame += "\x1b[?2026h\x1b[?25l\x1b[1;" + std::to_string(height) + "r";
        if (scrollLines > 0) {
            frame += "\x1b[1;1H\x1b[" + std::to_string(exposedLines) + "S";
            for (int row = height - exposedLines; row < height; ++row) {
                frame += "\x1b[" + std::to_string(row + 1) + ";1H";
                FormatHelpLine(help_scroll_y + row, width, frame);
            }
        } else {
            frame += "\x1b[1;1H\x1b[" + std::to_string(exposedLines) + "T";
            for (int row = 0; row < exposedLines; ++row) {
                frame += "\x1b[" + std::to_string(row + 1) + ";1H";
                FormatHelpLine(help_scroll_y + row, width, frame);
            }
        }
        frame += "\x1b[r";
        frame += "\x1b[?2026l";
        std::cout << frame;
        std::cout.flush();
    }

    void FormatBufferRow(int bufRow, int width, std::string& row) const {
        const int gutter = show_line_numbers ? 6 : 0;
        if (show_line_numbers) {
            std::string label = bufRow >= 0 && bufRow < static_cast<int>(active_buf->buffer.size()) ? std::to_string(bufRow + 1) : "~";
            if (label.size() < 5) label.insert(0, 5 - label.size(), ' ');
            row += "\x1b[90m" + label + " \x1b[0m";
        }
        width -= gutter;
        if (width < 1) return;
        if (bufRow >= (int)active_buf->buffer.size()) {
            row += "\x1b[90m~\x1b[0m";
            if (width > 1) {
                row.append(static_cast<size_t>(width - 1), ' ');
            }
            return;
        }

        const std::string& line = *active_buf->buffer[bufRow];
        const std::vector<SyntaxTokenType>& tokens = active_buf->highlight_tokens[bufRow];

        SyntaxTokenType lastToken = SyntaxTokenType::Default;
        row += ThemeTextAnsi();

        bool in_selection = false;
        int visual_col = 0;
        int char_idx = 0;
        const int line_len = static_cast<int>(line.size());

        // Skip characters that precede scroll_x visual column, handling split tab or wide character
        int split_spaces = 0;
        while (char_idx < line_len && visual_col < active_buf->scroll_x) {
            if (line[char_idx] == '\t') {
                const int tabWidth = tab_width - (visual_col % tab_width);
                if (visual_col + tabWidth > active_buf->scroll_x) {
                    split_spaces = (visual_col + tabWidth) - active_buf->scroll_x;
                    char_idx++;
                    break;
                }
                visual_col += tabWidth;
                char_idx++;
            } else {
                const int charWidth = Utf8DisplayWidth(line, char_idx);
                if (visual_col + charWidth > active_buf->scroll_x) {
                    split_spaces = (visual_col + charWidth) - active_buf->scroll_x;
                    char_idx = Utf8Next(line, char_idx);
                    break;
                }
                visual_col += charWidth;
                char_idx = Utf8Next(line, char_idx);
            }
        }

        int screen_col = 0;
        if (split_spaces > 0) {
            const int fill = (std::min)(split_spaces, width);
            row.append(static_cast<size_t>(fill), ' ');
            screen_col += fill;
        }

        for (; screen_col < width;) {
            if (char_idx < line_len) {
                const bool selected = IsSelectionCharacter(bufRow, char_idx);
                if (selected != in_selection) {
                    row += selected ? "\x1b[7m" : "\x1b[27m";
                    in_selection = selected;
                }

                SyntaxTokenType t = syntax_highlighting && char_idx < static_cast<int>(tokens.size()) ? tokens[char_idx] : SyntaxTokenType::Default;
                if (t != lastToken && !in_selection) {
                    row += Highlighter::GetTokenAnsi(t);
                    lastToken = t;
                }

                if (line[char_idx] == '\t') {
                    const int tabWidth = tab_width - ((active_buf->scroll_x + screen_col) % tab_width);
                    const int fill = (std::min)(tabWidth, width - screen_col);
                    row.append(static_cast<size_t>(fill), ' ');
                    screen_col += fill;
                    char_idx++;
                } else {
                    const int charWidth = Utf8DisplayWidth(line, char_idx);
                    const int next = Utf8Next(line, char_idx);
                    if (screen_col + charWidth <= width) {
                        row.append(line, static_cast<size_t>(char_idx), static_cast<size_t>(next - char_idx));
                        screen_col += (std::max)(1, charWidth);
                    } else {
                        row.append(static_cast<size_t>(width - screen_col), ' ');
                        screen_col = width;
                    }
                    char_idx = next;
                }
            } else {
                if (in_selection) {
                    row += "\x1b[27m";
                    in_selection = false;
                }
                if (lastToken != SyntaxTokenType::Default) {
                    row += ThemeTextAnsi();
                    lastToken = SyntaxTokenType::Default;
                }
                const int remaining = width - screen_col;
                if (remaining > 0) {
                    row.append(static_cast<size_t>(remaining), ' ');
                }
                break;
            }
        }
        if (in_selection) row += "\x1b[27m";
        row += "\x1b[0m";
    }

    const char* ThemeTextAnsi() const {
        switch (current_theme) {
            case EveTheme::Orange:
            case EveTheme::Green:
            case EveTheme::Pink:
                return "\x1b[30m";
            default:
                return "\x1b[39m";
        }
    }

    // ANSI SGR for the status bar / divider background; "\x1b[7m" (plain reverse video)
    // is the Default theme so unthemed terminals look exactly as before.
    std::string ThemeStatusAnsi() const {
        switch (current_theme) {
            case EveTheme::Black:  return "\x1b[97;40m";
            case EveTheme::Orange: return "\x1b[30;48;5;208m";
            case EveTheme::Green:  return "\x1b[30;42m";
            case EveTheme::Red:    return "\x1b[97;41m";
            case EveTheme::Blue:   return "\x1b[97;44m";
            case EveTheme::Pink:   return "\x1b[30;48;5;213m";
            default:               return "\x1b[7m";
        }
    }

    static const char* ThemeName(EveTheme theme) {
        switch (theme) {
            case EveTheme::Black:  return "BLACK";
            case EveTheme::Orange: return "ORANGE";
            case EveTheme::Green:  return "GREEN";
            case EveTheme::Red:    return "RED";
            case EveTheme::Blue:   return "BLUE";
            case EveTheme::Pink:   return "PINK";
            default:                return "DEFAULT";
        }
    }

    std::string BuildStatusLine(int width) const {
        std::string lang_tag = "[" + Highlighter::GetLangName(active_buf->current_lang) + "]";
        std::string status = " Buffer: " + active_buf->buffer_name + " (" + active_buf->filename + ";" +
            std::to_string(active_buf->save_version) + (active_buf->modified ? " *" : "") + ") " + lang_tag;
        while (status.size() < 42) status += " ";
        status += "| " + std::string(active_buf->read_only ? "Read-only" : "Write") + " | " + 
                  std::string(active_buf->insert_mode ? "Insert" : "Overstrike") + " | " + 
                  (active_buf->forward_dir ? "Forward" : "Reverse") + " | Ln " +
                  std::to_string(active_buf->cursor_y + 1) + "/" + std::to_string(active_buf->buffer.size()) +
                  ", Col " + std::to_string(VisualColumn(active_buf->cursor_y, active_buf->cursor_x) + 1);
        if (status.size() < (size_t)width) status.append(width - status.size(), ' ');
        if (status.size() > (size_t)width) status = status.substr(0, width);
        return status;
    }

    std::string BuildFooterLine(int width) const {
        std::string footer = in_command_mode ? ("Command: " + command_input) :
            (gold_armed ? ("GOLD-  " + message) : message);
        if (footer.size() < (size_t)width) footer.append(width - footer.size(), ' ');
        if (footer.size() > (size_t)width) footer = footer.substr(0, width);
        return footer;
    }

    void Render() {
        int width, height;
        GetWindowSize(width, height);

        if (in_help_mode) {
            rendered_valid = false;
            const int helpScrollLines = pending_help_scroll_lines;
            pending_help_scroll_lines = 0;
            if (helpScrollLines == 0) RenderHelp(width, height);
            else RenderScrolledHelp(width, height, helpScrollLines);
            return;
        }

        int text_height = height - 2;
        if (text_height < 1 || width < 2) return;

        const int primary_height = split_view ? (std::max)(1, (text_height - 1) / 2) : text_height;

        if (active_buf->cursor_y < active_buf->scroll_y) active_buf->scroll_y = active_buf->cursor_y;
        if (active_buf->cursor_y >= active_buf->scroll_y + primary_height) active_buf->scroll_y = active_buf->cursor_y - primary_height + 1;

        const int cursor_vcol = VisualColumn(active_buf->cursor_y, active_buf->cursor_x);
        if (cursor_vcol < active_buf->scroll_x) active_buf->scroll_x = cursor_vcol;
        if (cursor_vcol >= active_buf->scroll_x + width) active_buf->scroll_x = cursor_vcol - width + 1;
        if (word_wrap) active_buf->scroll_x = 0;

        EnsureHighlightThrough(active_buf->scroll_y + primary_height - 1);
        if (split_view) {
            other_scroll_y = (std::max)(0, (std::min)(other_scroll_y, (std::max)(0, (int)active_buf->buffer.size() - 1)));
            EnsureHighlightThrough(other_scroll_y + (text_height - 1 - primary_height) - 1);
        }

        const std::string status = BuildStatusLine(width);
        const std::string footer = BuildFooterLine(width);
        const bool selection = HasTextSelection();
        const int scrollDelta = active_buf->scroll_y - rendered_scroll_y;

        // Only the viewport moved: scroll the region and repaint the exposed rows.
        const bool scrollOnly = !split_view && rendered_valid && scrollDelta != 0 &&
            scrollDelta > -text_height && scrollDelta < text_height &&
            width == rendered_width && height == rendered_height &&
            active_buf->scroll_x == rendered_scroll_x && active_buf->buffer_version == rendered_buffer_version &&
            !selection && !rendered_selection &&
            status == rendered_status && footer == rendered_footer;

        frame_buffer.clear();
        if (frame_buffer.capacity() < 65536) {
            frame_buffer.reserve(65536);
        }
        frame_buffer += "\x1b[?2026h\x1b[?25l";

        if (scrollOnly) {
            const int exposed = scrollDelta < 0 ? -scrollDelta : scrollDelta;
            frame_buffer += "\x1b[1;" + std::to_string(text_height) + "r";
            if (scrollDelta > 0) {
                frame_buffer += "\x1b[1;1H\x1b[" + std::to_string(exposed) + "S";
                for (int row = text_height - exposed; row < text_height; ++row) {
                    frame_buffer += "\x1b[" + std::to_string(row + 1) + ";1H";
                    FormatBufferRow(active_buf->scroll_y + row, width, frame_buffer);
                }
            } else {
                frame_buffer += "\x1b[1;1H\x1b[" + std::to_string(exposed) + "T";
                for (int row = 0; row < exposed; ++row) {
                    frame_buffer += "\x1b[" + std::to_string(row + 1) + ";1H";
                    FormatBufferRow(active_buf->scroll_y + row, width, frame_buffer);
                }
            }
            frame_buffer += "\x1b[r";
        } else if (split_view) {
            const int secondary_height = text_height - 1 - primary_height;
            frame_buffer += "\x1b[H";
            for (int r = 0; r < primary_height; ++r) {
                FormatBufferRow(active_buf->scroll_y + r, width, frame_buffer);
                frame_buffer += "\n";
            }
            std::string divider = "--- Other Window: " + active_buf->buffer_name +
                " (GOLD-9/GOLD-0 scroll, GOLD-2 close) ---";
            if ((int)divider.size() < width) divider.append(width - divider.size(), '-');
            else if ((int)divider.size() > width) divider = divider.substr(0, width);
            frame_buffer += ThemeStatusAnsi() + divider + "\x1b[0m\n";
            for (int r = 0; r < secondary_height; ++r) {
                FormatBufferRow(other_scroll_y + r, width, frame_buffer);
                frame_buffer += "\n";
            }
            frame_buffer += ThemeStatusAnsi() + status + "\x1b[0m\n";
            frame_buffer += footer;
        } else {
            frame_buffer += "\x1b[H";
            for (int r = 0; r < text_height; ++r) {
                FormatBufferRow(active_buf->scroll_y + r, width, frame_buffer);
                frame_buffer += "\n";
            }
            frame_buffer += ThemeStatusAnsi() + status + "\x1b[0m\n";
            frame_buffer += footer;
        }

        if (in_command_mode) {
            int cx = 10 + (int)command_input.size();
            frame_buffer += "\x1b[" + std::to_string(height) + ";" + std::to_string(cx) + "H";
        } else {
            int cx = (cursor_vcol - active_buf->scroll_x) + 1 + (show_line_numbers ? 6 : 0);
            int cy = (active_buf->cursor_y - active_buf->scroll_y) + 1;
            frame_buffer += "\x1b[" + std::to_string(cy) + ";" + std::to_string(cx) + "H";
        }
        frame_buffer += "\x1b[?25h\x1b[?2026l";

        std::cout << frame_buffer;
        std::cout.flush();

        rendered_valid = true;
        rendered_buffer_version = active_buf->buffer_version;
        rendered_scroll_x = active_buf->scroll_x;
        rendered_scroll_y = active_buf->scroll_y;
        rendered_width = width;
        rendered_height = height;
        rendered_selection = selection;
        rendered_status = status;
        rendered_footer = footer;
    }

    static bool IsWordChar(uint32_t cp) {
        return (cp < 256 && std::isalnum(static_cast<unsigned char>(cp))) || cp == '_';
    }

    void MoveWordLeft() {
        if (active_buf->cursor_x == 0) {
            if (active_buf->cursor_y > 0) {
                active_buf->cursor_y--;
                active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
            }
            return;
        }

        const std::string line = GetLine(*active_buf, active_buf->cursor_y);
        int x = active_buf->cursor_x;

        // Skip leading whitespace / non-alphanumeric going left
        while (x > 0) {
            int prev = Utf8Previous(line, x);
            uint32_t cp = Utf8CodePoint(line, prev);
            if (IsWordChar(cp)) break;
            x = prev;
        }

        // Skip alphanumeric going left
        while (x > 0) {
            int prev = Utf8Previous(line, x);
            uint32_t cp = Utf8CodePoint(line, prev);
            if (!IsWordChar(cp)) break;
            x = prev;
        }

        active_buf->cursor_x = x;
    }

    void MoveWordRight() {
        const std::string line = GetLine(*active_buf, active_buf->cursor_y);
        if (active_buf->cursor_x >= (int)line.size()) {
            if (active_buf->cursor_y < (int)active_buf->buffer.size() - 1) {
                active_buf->cursor_y++;
                active_buf->cursor_x = 0;
            }
            return;
        }

        int x = active_buf->cursor_x;
        int len = (int)line.size();

        // Skip leading alphanumeric going right
        while (x < len) {
            uint32_t cp = Utf8CodePoint(line, x);
            if (!IsWordChar(cp)) break;
            x = Utf8Next(line, x);
        }

        // Skip whitespace / non-alphanumeric going right to find next word start
        while (x < len) {
            uint32_t cp = Utf8CodePoint(line, x);
            if (IsWordChar(cp)) break;
            x = Utf8Next(line, x);
        }

        active_buf->cursor_x = x;
    }

    void MoveParagraphUp() {
        if (active_buf->cursor_y <= 0) return;
        int y = active_buf->cursor_y - 1;
        bool start_blank = Trim(GetLine(*active_buf, active_buf->cursor_y)).empty();
        if (start_blank) {
            while (y > 0 && Trim(GetLine(*active_buf, y)).empty()) y--;
        }
        while (y > 0 && !Trim(GetLine(*active_buf, y)).empty()) y--;
        active_buf->cursor_y = y;
        if (active_buf->cursor_x > (int)GetLine(*active_buf, active_buf->cursor_y).size()) active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
    }

    void MoveParagraphDown() {
        if (active_buf->cursor_y >= (int)active_buf->buffer.size() - 1) return;
        int y = active_buf->cursor_y + 1;
        bool start_blank = Trim(GetLine(*active_buf, active_buf->cursor_y)).empty();
        if (start_blank) {
            while (y < (int)active_buf->buffer.size() - 1 && Trim(GetLine(*active_buf, y)).empty()) y++;
        }
        while (y < (int)active_buf->buffer.size() - 1 && !Trim(GetLine(*active_buf, y)).empty()) y++;
        active_buf->cursor_y = y;
        if (active_buf->cursor_x > (int)GetLine(*active_buf, active_buf->cursor_y).size()) active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
    }

    void MoveCursorLeft() {
        if (active_buf->cursor_x > 0) {
            active_buf->cursor_x = Utf8Previous(GetLine(*active_buf, active_buf->cursor_y), active_buf->cursor_x);
        } else if (active_buf->cursor_y > 0) {
            active_buf->cursor_y--;
            active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
        }
    }

    void MoveCursorRight() {
        if (active_buf->cursor_x < (int)GetLine(*active_buf, active_buf->cursor_y).size()) {
            active_buf->cursor_x = Utf8Next(GetLine(*active_buf, active_buf->cursor_y), active_buf->cursor_x);
        } else if (active_buf->cursor_y < (int)active_buf->buffer.size() - 1) {
            active_buf->cursor_y++;
            active_buf->cursor_x = 0;
        }
    }

    void MoveCursorUp() {
        if (active_buf->cursor_y > 0) {
            active_buf->cursor_y--;
            if (active_buf->cursor_x > (int)GetLine(*active_buf, active_buf->cursor_y).size()) {
                active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
            }
        }
    }

    void MoveCursorDown() {
        if (active_buf->cursor_y < (int)active_buf->buffer.size() - 1) {
            active_buf->cursor_y++;
            if (active_buf->cursor_x > (int)GetLine(*active_buf, active_buf->cursor_y).size()) {
                active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
            }
        }
    }

    void ScrollBy(int lines) {
        int width, height;
        GetWindowSize(width, height);
        int max_scroll = (std::max)(0, (int)active_buf->buffer.size() - (height - 2));

        active_buf->scroll_y = (std::max)(0, (std::min)(max_scroll, active_buf->scroll_y + lines));
        active_buf->cursor_y = (std::max)(0, (std::min)((int)active_buf->buffer.size() - 1, active_buf->cursor_y + lines));
        if (active_buf->cursor_x > (int)GetLine(*active_buf, active_buf->cursor_y).size()) active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
    }

    void ScrollHelpBy(int lines) {
        int width, height;
        GetWindowSize(width, height);
        const int max_scroll = (std::max)(0, static_cast<int>(HelpLines().size()) - height);
        const int previous_scroll = help_scroll_y;
        help_scroll_y = (std::max)(0, (std::min)(max_scroll, help_scroll_y + lines));
        pending_help_scroll_lines += help_scroll_y - previous_scroll;
    }

    void AdvanceExitConfirm(bool save) {
        if (!in_exit_confirm || exit_confirm_cursor >= exit_confirm_pending.size()) return;
        LoadActiveBufferState(exit_confirm_pending[exit_confirm_cursor]);
        if (save && !SaveFile(active_buf->filename)) {
            // Abort EXIT instead of discarding this buffer's changes silently; SaveFile
            // already set an explanatory %EVE-E-... message.
            in_exit_confirm = false;
            exit_confirm_pending.clear();
            return;
        }
        ++exit_confirm_cursor;
        if (exit_confirm_cursor >= exit_confirm_pending.size()) {
            in_exit_confirm = false;
            running = false;
        } else {
            LoadActiveBufferState(exit_confirm_pending[exit_confirm_cursor]);
            message = "Save changes to " + active_buf->buffer_name + "? (Y/N, Ctrl+C to cancel EXIT)";
        }
    }

    void CancelExitConfirm() {
        in_exit_confirm = false;
        exit_confirm_pending.clear();
        message = "%EVE-I-CANCELLED, EXIT cancelled";
    }

    static const std::vector<std::string>& CanonicalCommands() {
        static const std::vector<std::string> commands = {
            "HELP", "EXIT", "QUIT", "WRITE", "SAVE", "OPEN", "GET", "INCLUDE", "CLOSE", "SHOW", "BUFFERS",
            "BUFFER", "TOP", "BOTTOM", "RELOAD", "REVERT", "MARK", "GOTO", "LINE", "FIND", "FINDNEXT", "NEXT",
            "UNDO", "REDO", "REPLACE", "SUBSTITUTE", "SUBST", "SET", "FORWARD", "REVERSE", "INSERT",
            "OVERSTRIKE", "CHANGE", "SELECT", "REMOVE", "CUT", "COPY", "PASTE", "FILL", "RECOVER", "LEARN",
            "EXECUTE", "SPLIT", "THEME"
        };
        return commands;
    }

    // DCL-style abbreviation: expands an unambiguous command prefix to its full canonical form.
    bool ExpandCommandAbbreviation(std::string& verb, std::string& error) const {
        const auto& commands = CanonicalCommands();
        if (verb == "EX" || verb == "Q") return true; // established short aliases stay as-is
        if (std::find(commands.begin(), commands.end(), verb) != commands.end()) return true;
        std::vector<std::string> matches;
        for (const auto& command : commands) {
            if (command.rfind(verb, 0) == 0) matches.push_back(command);
        }
        if (matches.size() == 1) { verb = matches.front(); return true; }
        if (matches.size() > 1) {
            error = "%EVE-E-AMBIGUOUS, Ambiguous command \"" + verb + "\" (";
            for (size_t i = 0; i < matches.size(); ++i) { if (i) error += ", "; error += matches[i]; }
            error += ")";
            return false;
        }
        return true; // no canonical match; fall through to the unrecognized-command handler
    }

    void ExecuteCommand(const std::string& cmd_str) {
        std::string trimmed = Trim(cmd_str);
        if (trimmed.empty()) return;

        std::stringstream ss(trimmed);
        std::string verb;
        ss >> verb;
        verb = ToUpper(verb);

        std::string rest;
        std::getline(ss, rest);
        rest = Trim(rest);

        std::string abbrevError;
        if (!ExpandCommandAbbreviation(verb, abbrevError)) { message = abbrevError; return; }

        if (verb == "HELP") {
            in_help_mode = true;
            help_scroll_y = 0;
            pending_help_scroll_lines = 0;
        } else if (verb == "EXIT" || verb == "EX") {
            exit_confirm_pending.clear();
            for (size_t i = 0; i < buffers.size(); ++i) {
                if (buffers[i].modified) exit_confirm_pending.push_back(i);
            }
            if (exit_confirm_pending.empty()) {
                running = false;
            } else {
                exit_confirm_cursor = 0;
                in_exit_confirm = true;
                LoadActiveBufferState(exit_confirm_pending[0]);
                message = "Save changes to " + active_buf->buffer_name + "? (Y/N, Ctrl+C to cancel EXIT)";
            }
        } else if (verb == "QUIT" || verb == "Q") {
            bool hasModified = false;
            for (const auto& buffer : buffers) hasModified = hasModified || buffer.modified;
            if (hasModified && ToUpper(rest) != "/FORCE") {
                message = "%EVE-W-MODIFIED, Unsaved changes; use QUIT /FORCE to discard them";
            } else {
                running = false;
            }
        } else if (verb == "WRITE" || verb == "SAVE") {
            SaveFile(rest.empty() ? active_buf->filename : rest);
        } else if (verb == "OPEN" || verb == "GET" || verb == "INCLUDE") {
            if (rest.empty()) {
                message = "%EVE-E-SYNTAX, OPEN requires a filename";
            } else {
                int found_idx = -1;
                for (size_t i = 0; i < buffers.size(); ++i) {
                    std::string f1 = buffers[i].filename;
                    std::string f2 = rest;
                    std::transform(f1.begin(), f1.end(), f1.begin(), ::toupper);
                    std::transform(f2.begin(), f2.end(), f2.begin(), ::toupper);
                    if (f1 == f2) {
                        found_idx = static_cast<int>(i);
                        break;
                    }
                }
                
                if (found_idx != -1) {
                    LoadActiveBufferState(static_cast<size_t>(found_idx));
                    message = "Switched to buffer: " + active_buf->buffer_name;
                } else {
                    BufferState b;
                    b.filename = rest;
                    b.buffer_name = GetBufferNameFromPath(rest);
                    
                    LoadBufferFromFile(b, rest);
                    b.current_lang = Highlighter::DetectLanguage(rest);
                    b.highlight_tokens.resize(b.buffer.size());
                    b.highlight_end_states.resize(b.buffer.size(), HighlightState{});
                    b.highlight_valid.resize(b.buffer.size(), false);
                    
                    DWORD attrib = GetFileAttributesA(rest.c_str());
                    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_READONLY)) {
                        b.read_only = true;
                    }
                    
                    buffers.push_back(std::move(b));
                    LoadActiveBufferState(buffers.size() - 1);
                    message = "Opened new buffer: " + active_buf->buffer_name;
                }
            }
        } else if (verb == "CLOSE") {
            if (buffers.size() == 1) {
                message = "%EVE-E-LASTBUFFER, Cannot close the only buffer";
            } else if (active_buf->modified && ToUpper(rest) != "/FORCE") {
                message = "%EVE-W-MODIFIED, Save changes or use CLOSE /FORCE";
            } else {
                buffers.erase(buffers.begin() + active_buffer_index);
                LoadActiveBufferState(active_buffer_index >= buffers.size() ? buffers.size() - 1 : active_buffer_index);
                message = "Buffer closed: " + active_buf->buffer_name;
            }
        } else if (verb == "SHOW" && ToUpper(rest) == "BUFFERS") {
            std::string msg = "Buffers: ";
            for (size_t i = 0; i < buffers.size(); ++i) {
                if (i > 0) msg += ", ";
                msg += (i == active_buffer_index) ? "[" + buffers[i].buffer_name + "]" : buffers[i].buffer_name;
            }
            message = msg;
        } else if (verb == "BUFFERS") {
            std::string msg = "Buffers: ";
            for (size_t i = 0; i < buffers.size(); ++i) {
                if (i > 0) msg += ", ";
                msg += (i == active_buffer_index) ? "[" + buffers[i].buffer_name + "]" : buffers[i].buffer_name;
            }
            message = msg;
        } else if (verb == "BUFFER") {
            if (rest.empty()) {
                message = "Current buffer: " + active_buf->buffer_name;
            } else {
                std::string target = ToUpper(rest);
                int found_idx = -1;
                for (size_t i = 0; i < buffers.size(); ++i) {
                    if (buffers[i].buffer_name == target) {
                        found_idx = static_cast<int>(i);
                        break;
                    }
                }
                if (found_idx != -1) {
                    LoadActiveBufferState(static_cast<size_t>(found_idx));
                    message = "Switched to buffer: " + active_buf->buffer_name;
                } else {
                    LoadActiveBufferState(active_buffer_index);
                    message = "%EVE-E-NOBUFFER, No buffer named " + target;
                }
            }
        } else if (verb == "TOP") {
            active_buf->cursor_y = 0; active_buf->cursor_x = 0;
            message = "Top of buffer";
        } else if (verb == "BOTTOM" || verb == "BOT") {
            active_buf->cursor_y = (int)active_buf->buffer.size() - 1;
            active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
            message = "Bottom of buffer";
        } else if (verb == "RELOAD" || verb == "REVERT") {
            if (active_buf->modified && ToUpper(rest) != "/FORCE") {
                message = "%EVE-W-MODIFIED, Use RELOAD /FORCE to discard buffer changes";
            } else {
                LoadFile(active_buf->filename);
            }
        } else if (verb == "MARK") {
            if (rest.empty()) message = "%EVE-E-SYNTAX, MARK requires a name";
            else { active_buf->bookmarks[ToUpper(rest)] = active_buf->cursor_y; message = "Marked " + ToUpper(rest); }
        } else if (verb == "SHOW" && ToUpper(rest) == "MARKS") {
            std::string marks = "Marks: ";
            for (const auto& [name, line] : active_buf->bookmarks) marks += name + "=" + std::to_string(line + 1) + " ";
            message = marks;
        } else if (verb == "GOTO" && ToUpper(rest).rfind("MARK ", 0) == 0) {
            const std::string name = ToUpper(Trim(rest.substr(5)));
            auto mark = active_buf->bookmarks.find(name);
            if (mark == active_buf->bookmarks.end()) message = "%EVE-E-NOMARK, No mark named " + name;
            else { active_buf->cursor_y = mark->second; active_buf->cursor_x = 0; message = "Mark " + name; }
        } else if (verb == "GOTO" || verb == "LINE") {
            try {
                std::string num_part = rest;
                if (verb == "GOTO" && rest.size() >= 5 && ToUpper(rest.substr(0, 5)) == "LINE ") {
                    num_part = Trim(rest.substr(5));
                }
                int line = std::stoi(num_part);
                if (line > 0 && line <= (int)active_buf->buffer.size()) {
                    active_buf->cursor_y = line - 1;
                    active_buf->cursor_x = 0;
                    message = "Line " + std::to_string(line);
                } else {
                    message = "%EVE-E-OUTRANGE, Line number out of range";
                }
            } catch (...) { message = "%EVE-E-SYNTAX, Invalid line number"; }
        } else if (verb == "FIND") {
            FindText(rest);
        } else if (verb == "FINDNEXT" || verb == "NEXT") {
            FindText(last_search);
        } else if (verb == "UNDO") {
            Undo();
        } else if (verb == "REDO") {
            Redo();
        } else if (verb == "REPLACE" || verb == "SUBSTITUTE" || verb == "SUBST") {
            ReplaceText(rest);
        } else if (verb == "FILL") {
            FillParagraph();
        } else if (verb == "RECOVER") {
            RecoverFromJournal();
        } else if (verb == "LEARN") {
            ToggleLearn();
        } else if (verb == "EXECUTE") {
            ExecuteLearn();
        } else if (verb == "SPLIT") {
            split_view = !split_view;
            if (split_view) other_scroll_y = active_buf->scroll_y;
            rendered_valid = false;
            message = split_view ? "Split window enabled" : "Split window disabled";
        } else if (verb == "THEME") {
            static const std::unordered_map<std::string, EveTheme> themes = {
                {"DEFAULT", EveTheme::Default}, {"BLACK", EveTheme::Black}, {"ORANGE", EveTheme::Orange},
                {"GREEN", EveTheme::Green}, {"RED", EveTheme::Red}, {"BLUE", EveTheme::Blue}, {"PINK", EveTheme::Pink}
            };
            const std::string color = ToUpper(rest);
            if (color.empty()) {
                message = "%EVE-E-SYNTAX, THEME requires a color: default, black, orange, green, red, blue, pink";
            } else {
                auto found = themes.find(color);
                if (found == themes.end()) {
                    message = "%EVE-E-UNKNOWN, Unknown theme \"" + rest + "\" (default, black, orange, green, red, blue, pink)";
                } else {
                    current_theme = found->second;
                    rendered_valid = false;
                    message = "Theme: " + std::string(ThemeName(current_theme));
                }
            }
        } else if (verb == "SET") {
            std::string opt = ToUpper(rest);
            if (opt == "INSERT") { active_buf->insert_mode = true; message = "Insert mode"; }
            else if (opt == "OVERSTRIKE") { active_buf->insert_mode = false; message = "Overstrike mode"; }
            else if (opt == "FORWARD") { active_buf->forward_dir = true; message = "Forward direction"; }
            else if (opt == "REVERSE") { active_buf->forward_dir = false; message = "Reverse direction"; }
            else if (opt == "READ-ONLY" || opt == "READONLY") { active_buf->read_only = true; message = "Read-only mode"; }
            else if (opt == "WRITE") { active_buf->read_only = false; message = "Write mode"; }
            else if (opt == "CASE ON") { search_case_sensitive = true; message = "Case-sensitive search"; }
            else if (opt == "CASE OFF") { search_case_sensitive = false; message = "Case-insensitive search"; }
            else if (opt == "WORD ON") { search_whole_word = true; message = "Whole-word search"; }
            else if (opt == "WORD OFF") { search_whole_word = false; message = "Substring search"; }
            else if (opt == "LINE NUMBERS ON") { show_line_numbers = true; message = "Line numbers enabled"; }
            else if (opt == "LINE NUMBERS OFF") { show_line_numbers = false; message = "Line numbers disabled"; }
            else if (opt == "HIGHLIGHT ON") { syntax_highlighting = true; message = "Syntax highlighting enabled"; }
            else if (opt == "HIGHLIGHT OFF") { syntax_highlighting = false; message = "Syntax highlighting disabled"; }
            else if (opt == "WRAP ON") { word_wrap = true; message = "Word wrap enabled"; }
            else if (opt == "WRAP OFF") { word_wrap = false; message = "Word wrap disabled"; }
            else if (opt.rfind("TAB WIDTH ", 0) == 0) {
                try { tab_width = (std::max)(1, (std::min)(16, std::stoi(Trim(rest.substr(10))))); message = "Tab width: " + std::to_string(tab_width); }
                catch (...) { message = "%EVE-E-SYNTAX, Tab width must be 1 through 16"; }
            } else if (opt == "RECTANGLE ON") { rectangular_selection = true; message = "Rectangular selection enabled"; }
            else if (opt == "RECTANGLE OFF") { rectangular_selection = false; message = "Stream selection enabled"; }
            else if (opt == "SPLIT ON") { split_view = true; other_scroll_y = active_buf->scroll_y; rendered_valid = false; message = "Split window enabled"; }
            else if (opt == "SPLIT OFF") { split_view = false; rendered_valid = false; message = "Split window disabled"; }
            else if (opt == "AUTO INDENT ON") { auto_indent = true; message = "Auto indent enabled"; }
            else if (opt == "AUTO INDENT OFF") { auto_indent = false; message = "Auto indent disabled"; }
            else if (opt.rfind("LEFT MARGIN ", 0) == 0) {
                try { left_margin = (std::max)(0, std::stoi(Trim(rest.substr(12)))); message = "Left margin: " + std::to_string(left_margin); }
                catch (...) { message = "%EVE-E-SYNTAX, Left margin must be a number"; }
            } else if (opt.rfind("RIGHT MARGIN ", 0) == 0) {
                try { right_margin = (std::max)(1, std::stoi(Trim(rest.substr(13)))); message = "Right margin: " + std::to_string(right_margin); }
                catch (...) { message = "%EVE-E-SYNTAX, Right margin must be a number"; }
            } else if (opt.rfind("LANGUAGE ", 0) == 0) {
                const std::string language = ToUpper(Trim(rest.substr(9)));
                static const std::vector<std::pair<std::string, Lang>> languages = {{"TEXT", Lang::Plain}, {"C", Lang::C_Cpp}, {"C++", Lang::C_Cpp}, {"PYTHON", Lang::Python}, {"JAVASCRIPT", Lang::JavaScript}, {"POWERSHELL", Lang::PowerShell}, {"SHELL", Lang::Shell}, {"BASIC", Lang::Basic}};
                auto found = std::find_if(languages.begin(), languages.end(), [&language](const auto& entry) { return entry.first == language; });
                if (found == languages.end()) message = "%EVE-E-UNKNOWN, Unknown language";
                else { active_buf->current_lang = found->second; InvalidateHighlightFrom(0); message = "Language: " + Highlighter::GetLangName(found->second); }
            }
            else message = "%EVE-E-UNKNOWN, Unknown SET qualifier";
        } else if (verb == "FORWARD") {
            active_buf->forward_dir = true;
            message = "Forward direction";
        } else if (verb == "REVERSE") {
            active_buf->forward_dir = false;
            message = "Reverse direction";
        } else if (verb == "INSERT") {
            active_buf->insert_mode = true;
            message = "Insert mode";
        } else if (verb == "OVERSTRIKE") {
            active_buf->insert_mode = false;
            message = "Overstrike mode";
        } else if (verb == "CHANGE") {
            std::string opt = ToUpper(rest);
            if (opt == "DIRECTION") {
                active_buf->forward_dir = !active_buf->forward_dir;
                message = active_buf->forward_dir ? "Forward direction" : "Reverse direction";
            } else if (opt == "MODE") {
                active_buf->insert_mode = !active_buf->insert_mode;
                message = active_buf->insert_mode ? "Insert mode" : "Overstrike mode";
            } else {
                message = "%EVE-E-UNKNOWN, Unknown CHANGE qualifier";
            }
        } else if (verb == "SELECT") {
            selecting = true; sel_start_x = active_buf->cursor_x; sel_start_y = active_buf->cursor_y;
            message = "Select start marked";
        } else if (verb == "REMOVE" || verb == "CUT") {
            CutSelection();
        } else if (verb == "COPY") {
            CopySelection();
        } else if ((verb == "INSERT" && ToUpper(rest) == "HERE") || verb == "PASTE") {
            PasteClipboard();
        } else {
            message = "%EVE-E-UNREC, Unrecognized command: " + verb + " (Type HELP for list)";
        }
    }

    void FindText(const std::string& target) {
        if (target.empty()) return;
        last_search = target;
        const std::string needle = search_case_sensitive ? target : ToUpper(target);
        auto matches = [&](const std::string& line, size_t start, bool reverse) {
            const std::string haystack = search_case_sensitive ? line : ToUpper(line);
            size_t found = reverse ? haystack.rfind(needle, start) : haystack.find(needle, start);
            while (found != std::string::npos && search_whole_word) {
                const bool left = found == 0 || !IsWordChar(static_cast<unsigned char>(line[found - 1]));
                const size_t after = found + target.size();
                const bool right = after >= line.size() || !IsWordChar(static_cast<unsigned char>(line[after]));
                if (left && right) break;
                found = reverse ? (found == 0 ? std::string::npos : haystack.rfind(needle, found - 1)) : haystack.find(needle, found + 1);
            }
            return found;
        };
        if (active_buf->forward_dir) {
            for (int r = active_buf->cursor_y; r < (int)active_buf->buffer.size(); ++r) {
                size_t pos = matches(*active_buf->buffer[r], (r == active_buf->cursor_y ? active_buf->cursor_x + 1 : 0), false);
                if (pos != std::string::npos) {
                    active_buf->cursor_y = r; active_buf->cursor_x = (int)pos;
                    message = "Found: " + target;
                    return;
                }
            }
        } else {
            for (int r = active_buf->cursor_y; r >= 0; --r) {
                if (r == active_buf->cursor_y && active_buf->cursor_x == 0) continue;
                size_t pos = matches(*active_buf->buffer[r], (r == active_buf->cursor_y ? active_buf->cursor_x - 1 : std::string::npos), true);
                if (pos != std::string::npos) {
                    active_buf->cursor_y = r; active_buf->cursor_x = (int)pos;
                    message = "Found: " + target;
                    return;
                }
            }
        }
        message = "%EVE-W-NOTFOUND, String not found: " + target;
    }

    bool ParseCommandArguments(const std::string& input, std::vector<std::string>& args, std::string& error) const {
        args.clear();
        std::string current;
        bool inQuote = false;
        bool escaped = false;
        bool tokenStarted = false;

        for (char ch : input) {
            if (escaped) {
                current += ch;
                escaped = false;
                tokenStarted = true;
                continue;
            }
            if (inQuote && ch == '\\') {
                escaped = true;
                tokenStarted = true;
                continue;
            }
            if (ch == '"') {
                inQuote = !inQuote;
                tokenStarted = true;
                continue;
            }
            if (!inQuote && std::isspace((unsigned char)ch)) {
                if (tokenStarted) {
                    args.push_back(current);
                    current.clear();
                    tokenStarted = false;
                }
                continue;
            }
            current += ch;
            tokenStarted = true;
        }

        if (escaped) current += '\\';
        if (inQuote) {
            error = "%EVE-E-SYNTAX, Missing closing quote";
            return false;
        }
        if (tokenStarted) args.push_back(current);
        return true;
    }

    void ReplaceText(const std::string& spec) {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();

        std::vector<std::string> args;
        std::string error;
        if (!ParseCommandArguments(spec, args, error)) {
            message = error;
            return;
        }

        bool replaceAll = false;
        if (!args.empty() && ToUpper(args.front()) == "ALL") {
            replaceAll = true;
            args.erase(args.begin());
        }
        if (!args.empty() && ToUpper(args.back()) == "ALL") {
            replaceAll = true;
            args.pop_back();
        }

        if (args.size() != 2 || args[0].empty()) {
            message = "%EVE-E-SYNTAX, Use REPLACE <old> <new> or REPLACE ALL <old> <new>";
            return;
        }

        const std::string& target = args[0];
        const std::string& replacement = args[1];
        int firstChangedLine = -1;
        int replacements = 0;

        if (replaceAll) {
            for (int lineIndex = 0; lineIndex < static_cast<int>(active_buf->buffer.size()); ++lineIndex) {
                std::string line = GetLine(*active_buf, lineIndex);
                size_t pos = 0;
                bool changed = false;
                while ((pos = line.find(target, pos)) != std::string::npos) {
                    line.replace(pos, target.size(), replacement);
                    if (firstChangedLine < 0) firstChangedLine = lineIndex;
                    ++replacements;
                    pos += replacement.size();
                    changed = true;
                }
                if (changed) SetLine(*active_buf, lineIndex, std::move(line));
            }
        } else if (active_buf->forward_dir) {
            for (int lineIndex = active_buf->cursor_y; lineIndex < static_cast<int>(active_buf->buffer.size()); ++lineIndex) {
                std::string line = GetLine(*active_buf, lineIndex);
                size_t start = (lineIndex == active_buf->cursor_y) ? static_cast<size_t>(active_buf->cursor_x) : 0;
                size_t pos = line.find(target, start);
                if (pos != std::string::npos) {
                    line.replace(pos, target.size(), replacement);
                    SetLine(*active_buf, lineIndex, std::move(line));
                    active_buf->cursor_y = lineIndex;
                    active_buf->cursor_x = static_cast<int>(pos + replacement.size());
                    firstChangedLine = lineIndex;
                    replacements = 1;
                    break;
                }
            }
        } else {
            for (int lineIndex = active_buf->cursor_y; lineIndex >= 0; --lineIndex) {
                std::string line = GetLine(*active_buf, lineIndex);
                size_t pos = std::string::npos;
                if (lineIndex == active_buf->cursor_y) {
                    if (active_buf->cursor_x > 0) pos = line.rfind(target, active_buf->cursor_x - 1);
                } else {
                    pos = line.rfind(target);
                }
                if (pos != std::string::npos) {
                    line.replace(pos, target.size(), replacement);
                    SetLine(*active_buf, lineIndex, std::move(line));
                    active_buf->cursor_y = lineIndex;
                    active_buf->cursor_x = static_cast<int>(pos);
                    firstChangedLine = lineIndex;
                    replacements = 1;
                    break;
                }
            }
        }

        if (replacements == 0) {
            message = "%EVE-W-NOTFOUND, String not found: " + target;
            return;
        }

        active_buf->modified = true;
        InvalidateHighlightFrom(firstChangedLine);
        selecting = false;
        message = replaceAll ? std::to_string(replacements) + " replacements made" : "Replaced: " + target;
    }

    bool HasTextSelection() const {
        return selecting && (sel_start_x != active_buf->cursor_x || sel_start_y != active_buf->cursor_y);
    }

    void IndentSelection(bool removeIndent) {
        if (CheckReadOnly()) return;
        const int firstLine = HasTextSelection() ? (std::min)(sel_start_y, active_buf->cursor_y) : active_buf->cursor_y;
        const int lastLine = HasTextSelection() ? (std::max)(sel_start_y, active_buf->cursor_y) : active_buf->cursor_y;
        SaveUndoSnapshot();
        for (int line = firstLine; line <= lastLine; ++line) {
            std::string text = GetLine(*active_buf, line);
            if (removeIndent) {
                if (text.rfind("    ", 0) == 0) text.erase(0, 4);
                else if (!text.empty() && text[0] == '\t') text.erase(0, 1);
            } else {
                text.insert(0, "    ");
            }
            SetLine(*active_buf, line, std::move(text));
        }
        active_buf->modified = true;
        InvalidateHighlightFrom(firstLine);
        message = removeIndent ? "Selection outdented" : "Selection indented";
    }

    void GetSelectionBounds(int& startX, int& startY, int& endX, int& endY) const {
        const bool anchorBeforeCursor = sel_start_y < active_buf->cursor_y ||
            (sel_start_y == active_buf->cursor_y && sel_start_x <= active_buf->cursor_x);
        startX = anchorBeforeCursor ? sel_start_x : active_buf->cursor_x;
        startY = anchorBeforeCursor ? sel_start_y : active_buf->cursor_y;
        endX = anchorBeforeCursor ? active_buf->cursor_x : sel_start_x;
        endY = anchorBeforeCursor ? active_buf->cursor_y : sel_start_y;
    }

    bool IsSelectionCharacter(int line, int column) const {
        if (!HasTextSelection()) return false;

        int startX, startY, endX, endY;
        GetSelectionBounds(startX, startY, endX, endY);
        if (rectangular_selection) {
            const int left = (std::min)(sel_start_x, active_buf->cursor_x);
            const int right = (std::max)(sel_start_x, active_buf->cursor_x);
            const int top = (std::min)(sel_start_y, active_buf->cursor_y);
            const int bottom = (std::max)(sel_start_y, active_buf->cursor_y);
            return line >= top && line <= bottom && column >= left && column < right;
        }
        if (line < startY || line > endY) return false;
        if (startY == endY) return line == startY && column >= startX && column < endX;
        if (line == startY) return column >= startX;
        if (line == endY) return column < endX;
        return true;
    }

    bool CopySelection() {
        if (!HasTextSelection()) {
            clipboard = GetLine(*active_buf, active_buf->cursor_y);
            WriteSystemClipboard();
            message = std::to_string(clipboard.size()) + " characters copied from current line";
            return true;
        }

        int startX, startY, endX, endY;
        GetSelectionBounds(startX, startY, endX, endY);
        clipboard.clear();
        if (rectangular_selection) {
            const int left = (std::min)(sel_start_x, active_buf->cursor_x);
            const int right = (std::max)(sel_start_x, active_buf->cursor_x);
            const int top = (std::min)(sel_start_y, active_buf->cursor_y);
            const int bottom = (std::max)(sel_start_y, active_buf->cursor_y);
            for (int line = top; line <= bottom; ++line) {
                const std::string text = GetLine(*active_buf, line);
                if (line > top) clipboard += "\n";
                if (left < static_cast<int>(text.size())) clipboard += text.substr(left, right - left);
            }
            WriteSystemClipboard();
            selecting = false;
            message = std::to_string(clipboard.size()) + " characters copied";
            return true;
        }
        if (startY == endY) {
            clipboard = GetLine(*active_buf, startY).substr(startX, endX - startX);
        } else {
            clipboard = GetLine(*active_buf, startY).substr(startX);
            for (int line = startY + 1; line < endY; ++line) {
                clipboard += "\n" + GetLine(*active_buf, line);
            }
            clipboard += "\n" + GetLine(*active_buf, endY).substr(0, endX);
        }
        WriteSystemClipboard();
        selecting = false;
        message = std::to_string(clipboard.size()) + " characters copied";
        return true;
    }

    bool CheckReadOnly() {
        if (active_buf->read_only) {
            std::cout << "\a";
            std::cout.flush();
            message = "%EVE-E-READONLY, Buffer is read-only";
            return true;
        }
        return false;
    }

    void WriteSystemClipboard() const {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        const int chars = MultiByteToWideChar(CP_UTF8, 0, clipboard.data(), static_cast<int>(clipboard.size()), nullptr, 0);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(chars + 1) * sizeof(wchar_t));
        if (memory != nullptr) {
            wchar_t* text = static_cast<wchar_t*>(GlobalLock(memory));
            if (text != nullptr) {
                MultiByteToWideChar(CP_UTF8, 0, clipboard.data(), static_cast<int>(clipboard.size()), text, chars);
                text[chars] = L'\0';
                GlobalUnlock(memory);
                if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) GlobalFree(memory);
            } else {
                GlobalFree(memory);
            }
        }
        CloseClipboard();
    }

    bool ReadSystemClipboard() {
        if (!OpenClipboard(nullptr)) return false;
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (data == nullptr) { CloseClipboard(); return false; }
        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text == nullptr) { CloseClipboard(); return false; }
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (bytes > 1) {
            clipboard.assign(static_cast<size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, clipboard.data(), bytes, nullptr, nullptr);
            clipboard.pop_back();
        }
        GlobalUnlock(data);
        CloseClipboard();
        return true;
    }

    void CutSelection() {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();
        int changedLine = active_buf->cursor_y;
        if (!HasTextSelection()) {
            clipboard = GetLine(*active_buf, active_buf->cursor_y);
            if (active_buf->buffer.size() > 1) {
                active_buf->buffer.erase(active_buf->buffer.begin() + active_buf->cursor_y);
                if (active_buf->cursor_y < static_cast<int>(active_buf->highlight_tokens.size())) {
                    active_buf->highlight_tokens.erase(active_buf->highlight_tokens.begin() + active_buf->cursor_y);
                    active_buf->highlight_end_states.erase(active_buf->highlight_end_states.begin() + active_buf->cursor_y);
                    active_buf->highlight_valid.erase(active_buf->highlight_valid.begin() + active_buf->cursor_y);
                }
                if (active_buf->cursor_y >= static_cast<int>(active_buf->buffer.size())) active_buf->cursor_y = static_cast<int>(active_buf->buffer.size()) - 1;
            } else {
                SetLine(*active_buf, 0, "");
            }
            active_buf->cursor_x = 0;
        } else {
            int startX, startY, endX, endY;
            GetSelectionBounds(startX, startY, endX, endY);
            changedLine = startY;
            CopySelection();

            if (rectangular_selection) {
                const int left = (std::min)(sel_start_x, active_buf->cursor_x);
                const int right = (std::max)(sel_start_x, active_buf->cursor_x);
                const int top = (std::min)(sel_start_y, active_buf->cursor_y);
                const int bottom = (std::max)(sel_start_y, active_buf->cursor_y);
                for (int line = top; line <= bottom; ++line) {
                    std::string text = GetLine(*active_buf, line);
                    if (left < static_cast<int>(text.size())) {
                        text.erase(left, right - left);
                        SetLine(*active_buf, line, std::move(text));
                    }
                }
                changedLine = top;
                active_buf->cursor_y = top;
                active_buf->cursor_x = left;
            } else if (startY == endY) {
                std::string text = GetLine(*active_buf, startY);
                text.erase(startX, endX - startX);
                SetLine(*active_buf, startY, std::move(text));
            } else {
                SetLine(*active_buf, startY, GetLine(*active_buf, startY).substr(0, startX) + GetLine(*active_buf, endY).substr(endX));
                active_buf->buffer.erase(active_buf->buffer.begin() + startY + 1, active_buf->buffer.begin() + endY + 1);
                if (startY + 1 < static_cast<int>(active_buf->highlight_tokens.size())) {
                    size_t erase_end = (std::min)(active_buf->highlight_tokens.size(), static_cast<size_t>(endY + 1));
                    active_buf->highlight_tokens.erase(active_buf->highlight_tokens.begin() + startY + 1, active_buf->highlight_tokens.begin() + erase_end);
                    active_buf->highlight_end_states.erase(active_buf->highlight_end_states.begin() + startY + 1, active_buf->highlight_end_states.begin() + erase_end);
                    active_buf->highlight_valid.erase(active_buf->highlight_valid.begin() + startY + 1, active_buf->highlight_valid.begin() + erase_end);
                }
            }
            active_buf->cursor_y = startY;
            active_buf->cursor_x = startX;
        }
        InvalidateHighlightFrom(changedLine);
        active_buf->modified = true;
        WriteSystemClipboard();
        message = std::to_string(clipboard.size()) + " characters removed";
    }

    void PasteClipboard() {
        if (CheckReadOnly()) return;
        ReadSystemClipboard();
        if (clipboard.empty()) {
            message = "%EVE-E-NODATA, Clipboard is empty";
            return;
        }
        SaveUndoSnapshot();

        const int changedLine = active_buf->cursor_y;
        std::vector<std::string> parts;
        std::stringstream stream(clipboard);
        std::string part;
        while (std::getline(stream, part, '\n')) parts.push_back(part);
        if (!clipboard.empty() && clipboard.back() == '\n') parts.push_back("");

        std::string currentLine = GetLine(*active_buf, active_buf->cursor_y);
        const std::string suffix = currentLine.substr(active_buf->cursor_x);
        currentLine.erase(active_buf->cursor_x);
        currentLine += parts.front();

        if (parts.size() == 1) {
            currentLine += suffix;
            SetLine(*active_buf, active_buf->cursor_y, std::move(currentLine));
            active_buf->cursor_x += static_cast<int>(parts.front().size());
            InvalidateHighlightLine(changedLine);
        } else {
            SetLine(*active_buf, active_buf->cursor_y, std::move(currentLine));
            auto insertion = active_buf->buffer.begin() + active_buf->cursor_y + 1;
            for (size_t index = 1; index + 1 < parts.size(); ++index) {
                insertion = InsertLineAt(active_buf->buffer, insertion, parts[index]) + 1;
            }
            InsertLineAt(active_buf->buffer, insertion, parts.back() + suffix);
            active_buf->cursor_y += static_cast<int>(parts.size()) - 1;
            active_buf->cursor_x = static_cast<int>(parts.back().size());
            SyncHighlightStorage();
            InvalidateHighlightFrom(changedLine);
        }
        active_buf->modified = true;
        message = std::to_string(clipboard.size()) + " characters inserted";
    }

    void InsertStr(const std::string& s) {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();
        if (active_buf->cursor_y >= (int)active_buf->buffer.size()) PushLine(active_buf->buffer, "");
        std::string line = GetLine(*active_buf, active_buf->cursor_y);
        if (active_buf->cursor_x > (int)line.size()) line.append(active_buf->cursor_x - line.size(), ' ');

        if (active_buf->insert_mode) {
            line.insert(active_buf->cursor_x, s);
        } else {
            if (active_buf->cursor_x < (int)line.size()) {
                int next = Utf8Next(line, active_buf->cursor_x);
                line.erase(active_buf->cursor_x, next - active_buf->cursor_x);
            }
            line.insert(active_buf->cursor_x, s);
        }
        active_buf->cursor_x += (int)s.size();
        SetLine(*active_buf, active_buf->cursor_y, std::move(line));
        InvalidateHighlightLine(active_buf->cursor_y);
        active_buf->modified = true;
    }

    void InsertChar(char c) {
        InsertStr(std::string(1, c));
    }

    void NewLine() {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();
        std::string line = GetLine(*active_buf, active_buf->cursor_y);
        std::string rest = "";
        if (active_buf->cursor_x < (int)line.size()) {
            rest = line.substr(active_buf->cursor_x);
            line = line.substr(0, active_buf->cursor_x);
        }
        std::string indent;
        if (auto_indent) {
            const size_t nonBlank = line.find_first_not_of(" \t");
            indent = line.substr(0, nonBlank == std::string::npos ? line.size() : nonBlank);
        }
        if (left_margin > 0 && (int)indent.size() < left_margin) indent.append(left_margin - indent.size(), ' ');
        rest = indent + rest;
        SetLine(*active_buf, active_buf->cursor_y, std::move(line));
        InsertLineAt(active_buf->buffer, active_buf->buffer.begin() + active_buf->cursor_y + 1, rest);
        active_buf->cursor_y++;
        active_buf->cursor_x = (int)indent.size();
        SyncHighlightStorage();
        InvalidateHighlightFrom(active_buf->cursor_y - 1);
        active_buf->modified = true;
    }

    void Backspace() {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();
        if (active_buf->cursor_x > 0) {
            std::string line = GetLine(*active_buf, active_buf->cursor_y);
            const int previous = Utf8Previous(line, active_buf->cursor_x);
            line.erase(previous, active_buf->cursor_x - previous);
            active_buf->cursor_x = previous;
            SetLine(*active_buf, active_buf->cursor_y, std::move(line));
            active_buf->modified = true;
            InvalidateHighlightLine(active_buf->cursor_y);
        } else if (active_buf->cursor_y > 0) {
            active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y - 1).size();
            SetLine(*active_buf, active_buf->cursor_y - 1, GetLine(*active_buf, active_buf->cursor_y - 1) + GetLine(*active_buf, active_buf->cursor_y));
            active_buf->buffer.erase(active_buf->buffer.begin() + active_buf->cursor_y);
            if (active_buf->cursor_y < static_cast<int>(active_buf->highlight_tokens.size())) {
                active_buf->highlight_tokens.erase(active_buf->highlight_tokens.begin() + active_buf->cursor_y);
                active_buf->highlight_end_states.erase(active_buf->highlight_end_states.begin() + active_buf->cursor_y);
                active_buf->highlight_valid.erase(active_buf->highlight_valid.begin() + active_buf->cursor_y);
            }
            active_buf->cursor_y--;
            active_buf->modified = true;
            InvalidateHighlightFrom(active_buf->cursor_y);
        }
    }

    void Delete() {
        if (CheckReadOnly()) return;
        SaveUndoSnapshot();
        std::string line = GetLine(*active_buf, active_buf->cursor_y);
        if (active_buf->cursor_x < (int)line.size()) {
            line.erase(active_buf->cursor_x, Utf8Next(line, active_buf->cursor_x) - active_buf->cursor_x);
            SetLine(*active_buf, active_buf->cursor_y, std::move(line));
            active_buf->modified = true;
            InvalidateHighlightLine(active_buf->cursor_y);
        } else if (active_buf->cursor_y < (int)active_buf->buffer.size() - 1) {
            line += GetLine(*active_buf, active_buf->cursor_y + 1);
            SetLine(*active_buf, active_buf->cursor_y, std::move(line));
            active_buf->buffer.erase(active_buf->buffer.begin() + active_buf->cursor_y + 1);
            if (active_buf->cursor_y + 1 < static_cast<int>(active_buf->highlight_tokens.size())) {
                active_buf->highlight_tokens.erase(active_buf->highlight_tokens.begin() + active_buf->cursor_y + 1);
                active_buf->highlight_end_states.erase(active_buf->highlight_end_states.begin() + active_buf->cursor_y + 1);
                active_buf->highlight_valid.erase(active_buf->highlight_valid.begin() + active_buf->cursor_y + 1);
            }
            active_buf->modified = true;
            InvalidateHighlightFrom(active_buf->cursor_y);
        }
    }

    // Direction-sensitive character delete: Forward removes the char under the cursor
    // (like Delete); Reverse removes the char before it (like Backspace).
    void RemoveCharacterDirectional() {
        if (active_buf->forward_dir) Delete();
        else Backspace();
    }

    // Rewraps the paragraph around the cursor to fit between left_margin and right_margin,
    // mirroring EVE's FILL command.
    void FillParagraph() {
        if (CheckReadOnly()) return;
        int start = active_buf->cursor_y;
        while (start > 0 && !Trim(GetLine(*active_buf, start - 1)).empty()) --start;
        int end = active_buf->cursor_y;
        while (end + 1 < (int)active_buf->buffer.size() && !Trim(GetLine(*active_buf, end + 1)).empty()) ++end;
        if (Trim(GetLine(*active_buf, active_buf->cursor_y)).empty()) { message = "%EVE-W-NOTEXT, No paragraph at cursor to fill"; return; }

        SaveUndoSnapshot();
        std::string words;
        for (int line = start; line <= end; ++line) {
            if (!words.empty()) words += ' ';
            words += Trim(GetLine(*active_buf, line));
        }

        std::vector<std::string> filled;
        std::string indent(left_margin, ' ');
        std::string current = indent;
        std::stringstream stream(words);
        std::string word;
        while (stream >> word) {
            const size_t candidateLength = current.size() + (current.size() > indent.size() ? 1 : 0) + word.size();
            if (candidateLength > (size_t)right_margin && current.size() > indent.size()) {
                filled.push_back(current);
                current = indent + word;
            } else {
                if (current.size() > indent.size()) current += ' ';
                current += word;
            }
        }
        filled.push_back(current);

        active_buf->buffer.erase(active_buf->buffer.begin() + start, active_buf->buffer.begin() + end + 1);
        BufferType filledLines;
        filledLines.reserve(filled.size());
        for (auto& text : filled) PushLine(filledLines, std::move(text));
        active_buf->buffer.insert(active_buf->buffer.begin() + start, filledLines.begin(), filledLines.end());
        active_buf->cursor_y = start + (int)filled.size() - 1;
        active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size();
        SyncHighlightStorage();
        InvalidateHighlightFrom(start);
        active_buf->modified = true;
        message = "Filled " + std::to_string(filled.size()) + " lines";
    }

    static std::filesystem::path RecoveryDirectory() {
        std::error_code error;
        std::filesystem::path dir = std::filesystem::temp_directory_path(error) / "eve-recovery";
        if (!error) std::filesystem::create_directories(dir, error);
        return dir;
    }

    std::filesystem::path JournalPathFor(const BufferState& buffer) const {
        return RecoveryDirectory() / (UniqueRecoveryStem(buffer) + ".jou");
    }

    // Continuously flushes the active buffer's content to a journal file so it survives a
    // crash; distinct from WriteRecoveryFiles(), which only fires on abnormal termination.
    // Only the serialized text crosses to the background writer thread, never the live
    // buffer, so a slow disk/network share can't stall the interactive input loop.
    void MaybeWriteJournal() {
        if (active_buf->buffer_version == active_buf->journaled_version) return;
        std::string content;
        const char* newline = active_buf->use_crlf ? "\r\n" : "\n";
        for (const auto& line : active_buf->buffer) { content += *line; content += newline; }
        {
            std::lock_guard<std::mutex> lock(journal_writer_mutex);
            journal_writer_queue[JournalPathFor(*active_buf)] = std::move(content);
        }
        journal_writer_cv.notify_all();
        active_buf->journaled_version = active_buf->buffer_version;
    }

    void RecoverFromJournal() {
        if (CheckReadOnly()) return;
        const std::filesystem::path path = JournalPathFor(*active_buf);
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) { message = "%EVE-E-NOJOURNAL, No journal file found for " + active_buf->buffer_name; return; }
        SaveUndoSnapshot();
        active_buf->buffer.clear();
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            PushLine(active_buf->buffer, line);
        }
        if (active_buf->buffer.empty()) PushLine(active_buf->buffer, "");
        active_buf->cursor_x = active_buf->cursor_y = 0;
        active_buf->modified = true;
        SyncHighlightStorage();
        InvalidateHighlightFrom(0);
        message = "Recovered " + std::to_string(active_buf->buffer.size()) + " lines from journal";
    }

    void ToggleLearn() {
        if (!learning) {
            learn_buffer.clear();
            learning = true;
            message = "Learn sequence started (GOLD-K or LEARN to stop)";
        } else {
            learning = false;
            learn_has_content = !learn_buffer.empty();
            message = "Learn sequence stored (" + std::to_string(learn_buffer.size()) + " keystrokes); GOLD-E or EXECUTE to replay";
        }
    }

    void ExecuteLearn() {
        if (learning) { message = "%EVE-E-LEARNING, Stop the LEARN sequence (GOLD-K) before executing it"; return; }
        if (!learn_has_content || learn_buffer.empty()) { message = "%EVE-E-NOLEARN, No learn sequence has been recorded"; return; }
        replaying_learn = true;
        for (const KEY_EVENT_RECORD& recorded : learn_buffer) DispatchEditKey(recorded);
        replaying_learn = false;
        message = "Executed learn sequence (" + std::to_string(learn_buffer.size()) + " keystrokes)";
    }

    void HandleMouse(const MOUSE_EVENT_RECORD& mer) {
        int width, height;
        GetWindowSize(width, height);
        int text_height = height - 2;

        if (in_help_mode) {
            if (mer.dwEventFlags & MOUSE_WHEELED) {
                const short delta = static_cast<short>(HIWORD(mer.dwButtonState));
                if (delta > 0) ScrollHelpBy(-3);
                else if (delta < 0) ScrollHelpBy(3);
                return;
            }
            if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                in_help_mode = false;
            }
            return;
        }

        // Scroll Wheel Event
        if (mer.dwEventFlags & MOUSE_WHEELED) {
            short delta = (short)HIWORD(mer.dwButtonState);
            if (delta > 0) ScrollBy(-3); // Scroll Up
            else if (delta < 0) ScrollBy(3); // Scroll Down
            return;
        }

        // Left Click / Click & Drag
        if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
            int click_y = mer.dwMousePosition.Y;
            int click_x = mer.dwMousePosition.X;

            if (click_y < text_height) {
                int target_y = active_buf->scroll_y + click_y;
                int target_x = active_buf->scroll_x + click_x;

                int clamped_y = (std::max)(0, (std::min)(target_y, (int)active_buf->buffer.size() - 1));
                int clamped_x = VisualToByteIndex(GetLine(*active_buf, clamped_y), target_x);

                if (!(mer.dwEventFlags & MOUSE_MOVED)) {
                    sel_start_x = clamped_x;
                    sel_start_y = clamped_y;
                    selecting = false;
                } else {
                    selecting = true;
                }

                active_buf->cursor_y = clamped_y;
                active_buf->cursor_x = clamped_x;
                in_command_mode = false;
            } else if (click_y == height - 1) {
                in_command_mode = true;
            }
        }
    }

    void CoalescePendingMouseWheelEvents() {
        INPUT_RECORD pending{};
        DWORD available = 0;

        while (PeekConsoleInput(hIn, &pending, 1, &available) && available != 0 &&
               pending.EventType == MOUSE_EVENT &&
               (pending.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) != 0) {
            DWORD read = 0;
            if (!ReadConsoleInput(hIn, &pending, 1, &read) || read == 0) break;
            HandleMouse(pending.Event.MouseEvent);
        }
    }

    void CompleteCommandPath() {
        const size_t argumentStart = command_input.find_first_of(" \t");
        if (argumentStart == std::string::npos) return;
        const std::string prefix = Trim(command_input.substr(argumentStart + 1));
        const size_t separator = prefix.find_last_of("\\/");
        const std::string directory = separator == std::string::npos ? "." : prefix.substr(0, separator + 1);
        const std::string namePrefix = separator == std::string::npos ? prefix : prefix.substr(separator + 1);
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(directory.empty() ? "." : directory, error)) {
            const std::string name = entry.path().filename().string();
            if (name.rfind(namePrefix, 0) != 0) continue;
            command_input = command_input.substr(0, argumentStart + 1) + (separator == std::string::npos ? "" : prefix.substr(0, separator + 1)) + name;
            if (entry.is_directory(error)) command_input += "\\";
            return;
        }
    }

    // Dispatches classic GOLD-<key> EVE commands (e.g. GOLD-T = Top, GOLD-F = Find Next).
    // Returns false if the key has no GOLD binding, so the key can fall through normally.
    bool HandleGoldCommand(WORD vk, char ch) {
        (void)ch;
        switch (vk) {
            case 'T': ExecuteCommand("TOP"); return true;
            case 'B': ExecuteCommand("BOTTOM"); return true;
            case 'F': ExecuteCommand("FINDNEXT"); return true;
            case 'L': in_command_mode = true; command_input = "GOTO "; return true;
            case 'U': Undo(); return true;
            case 'Y': Redo(); return true;
            case 'W': SaveFile(active_buf->filename); return true;
            case 'R': CutSelection(); return true;
            case 'P': PasteClipboard(); return true;
            case 'D': RemoveCharacterDirectional(); return true;
            case 'J': FillParagraph(); return true;
            case 'K': ToggleLearn(); return true;
            case 'E': ExecuteLearn(); return true;
            case '2':
                split_view = !split_view;
                if (split_view) other_scroll_y = active_buf->scroll_y;
                rendered_valid = false;
                message = split_view ? "Split window enabled (GOLD-9/GOLD-0 scroll other window)" : "Split window disabled";
                return true;
            case '9':
                other_scroll_y = (std::max)(0, other_scroll_y - 10);
                message = "Other window scrolled up";
                return true;
            case '0':
                other_scroll_y = (std::min)((std::max)(0, (int)active_buf->buffer.size() - 1), other_scroll_y + 10);
                message = "Other window scrolled down";
                return true;
            case VK_OEM_PERIOD:
                selecting = true; sel_start_x = active_buf->cursor_x; sel_start_y = active_buf->cursor_y;
                message = "Select start marked";
                return true;
            case 'A':
                active_buf->insert_mode = !active_buf->insert_mode;
                message = active_buf->insert_mode ? "Insert mode" : "Overstrike mode";
                return true;
            case 'C':
                active_buf->forward_dir = !active_buf->forward_dir;
                message = active_buf->forward_dir ? "Forward direction" : "Reverse direction";
                return true;
            case 'H': in_help_mode = true; help_scroll_y = 0; pending_help_scroll_lines = 0; return true;
            default:
                message = "%EVE-I-NOGOLD, No GOLD command bound to that key";
                return true;
        }
    }

    // Handles a single cursor-movement/editing keystroke; factored out of Run() so the
    // GOLD-K/GOLD-E learn-execute macro feature can record and replay it.
    void DispatchEditKey(const KEY_EVENT_RECORD& ker) {
        if (learning && !replaying_learn) learn_buffer.push_back(ker);

        const char ch = ker.uChar.AsciiChar;
        const WORD vk = ker.wVirtualKeyCode;
        const DWORD ctrl = ker.dwControlKeyState;
        (void)ch;

        const bool shiftPressed = (ctrl & SHIFT_PRESSED) != 0;
        if (shiftPressed && (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT)) {
            if (!selecting) { selecting = true; sel_start_x = active_buf->cursor_x; sel_start_y = active_buf->cursor_y; }
        } else if (!shiftPressed && (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT)) {
            selecting = false;
        }
        switch (vk) {
            case VK_UP:    MoveCursorUp(); break;
            case VK_DOWN:  MoveCursorDown(); break;
            case VK_LEFT:  MoveCursorLeft(); break;
            case VK_RIGHT: MoveCursorRight(); break;
            case VK_HOME:  active_buf->cursor_x = 0; break;
            case VK_END:   active_buf->cursor_x = (int)GetLine(*active_buf, active_buf->cursor_y).size(); break;
            case VK_PRIOR: ScrollBy(-15); break; // Page Up
            case VK_NEXT:  ScrollBy(15); break;  // Page Down
            case VK_BACK:  Backspace(); break;
            case VK_DELETE:Delete(); break;
            case VK_RETURN:NewLine(); break;
            case VK_TAB:   InsertChar('\t'); break;
            case VK_F3:    FindText(last_search); break;
            case VK_F4:    search_whole_word = !search_whole_word; message = search_whole_word ? "Whole-word search" : "Substring search"; break;
            default: {
                wchar_t wc = ker.uChar.UnicodeChar;
                if (wc >= 0xD800 && wc <= 0xDBFF) {
                    pending_high_surrogate = wc;
                    return;
                }

                uint32_t codePoint = 0;
                if (pending_high_surrogate != 0) {
                    if (wc >= 0xDC00 && wc <= 0xDFFF) {
                        codePoint = 0x10000 + (((static_cast<uint32_t>(pending_high_surrogate) & 0x3FF) << 10) | (static_cast<uint32_t>(wc) & 0x3FF));
                        pending_high_surrogate = 0;
                    } else {
                        pending_high_surrogate = 0;
                        codePoint = wc;
                    }
                } else {
                    codePoint = wc;
                }

                if (codePoint >= 32) {
                    std::string utf8_char;
                    if (codePoint < 0x80) {
                        utf8_char.push_back(static_cast<char>(codePoint));
                    } else if (codePoint < 0x800) {
                        utf8_char.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                        utf8_char.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                    } else if (codePoint < 0x10000) {
                        utf8_char.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                        utf8_char.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                        utf8_char.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                    } else if (codePoint <= 0x10FFFF) {
                        utf8_char.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                        utf8_char.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                        utf8_char.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                        utf8_char.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                    }
                    InsertStr(utf8_char);
                }
                break;
            }
        }
    }

    void Run() {
        while (running && !g_consoleCtrlRequested.load(std::memory_order_relaxed)) {
            CheckExternalChanges();
            MaybeWriteJournal();
            if (needs_render) {
                Render();
                needs_render = false;
            }

            INPUT_RECORD ir;
            DWORD read;
            ReadConsoleInput(hIn, &ir, 1, &read);

            if (in_help_mode) {
                if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                    in_help_mode = false;
                    needs_render = true;
                } else if (ir.EventType == MOUSE_EVENT) {
                    HandleMouse(ir.Event.MouseEvent);
                    if ((ir.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) != 0) {
                        CoalescePendingMouseWheelEvents();
                        needs_render = pending_help_scroll_lines != 0;
                    } else {
                        needs_render = ir.Event.MouseEvent.dwButtonState != 0;
                    }
                }
                continue;
            }

            if (ir.EventType == MOUSE_EVENT) {
                HandleMouse(ir.Event.MouseEvent);
                if ((ir.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) != 0) {
                    CoalescePendingMouseWheelEvents();
                }
                needs_render = ir.Event.MouseEvent.dwButtonState != 0 ||
                    (ir.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) != 0;
                continue;
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
            needs_render = true;

            KEY_EVENT_RECORD ker = ir.Event.KeyEvent;
            char ch = ker.uChar.AsciiChar;
            WORD vk = ker.wVirtualKeyCode;
            DWORD ctrl = ker.dwControlKeyState;

            if (in_exit_confirm) {
                const char upperCh = (char)std::toupper((unsigned char)ch);
                if (upperCh == 'Y') AdvanceExitConfirm(true);
                else if (upperCh == 'N') AdvanceExitConfirm(false);
                else if (vk == 'C' && (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) CancelExitConfirm();
                continue;
            }

            // DO Key toggle: Ctrl+Z or ESC
            if ((vk == 'Z' && (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) || vk == VK_ESCAPE) {
                in_command_mode = !in_command_mode;
                command_input.clear();
                continue;
            }

            // GOLD key (F1, emulating the LK201 PF1/GOLD key): arms a one-shot
            // prefix so the next keystroke performs a classic EVE Gold-command.
            if (!in_command_mode && vk == VK_F1) {
                gold_armed = !gold_armed;
                if (gold_armed) message = "GOLD- (press a command key, or F1 to cancel)";
                continue;
            }
            if (gold_armed && !in_command_mode) {
                gold_armed = false;
                if (HandleGoldCommand(vk, ch)) continue;
            }

            if (in_command_mode) {
                if (vk == VK_RETURN) {
                    in_command_mode = false;
                    if (!Trim(command_input).empty()) {
                        command_history.push_back(command_input);
                        if (command_history.size() > 50) command_history.erase(command_history.begin());
                        ExecuteCommand(command_input);
                    }
                    command_input.clear();
                    command_history_index = -1;
                } else if (vk == VK_BACK) {
                    if (!command_input.empty()) command_input.pop_back();
                } else if (vk == VK_UP && !command_history.empty()) {
                    if (command_history_index < 0) command_history_index = static_cast<int>(command_history.size()) - 1;
                    else if (command_history_index > 0) --command_history_index;
                    command_input = command_history[command_history_index];
                } else if (vk == VK_DOWN && command_history_index >= 0) {
                    if (++command_history_index >= static_cast<int>(command_history.size())) { command_history_index = -1; command_input.clear(); }
                    else command_input = command_history[command_history_index];
                } else if (vk == VK_TAB) {
                    const auto& commands = CanonicalCommands();
                    const size_t wordEnd = command_input.find_first_of(" \t");
                    if (wordEnd == std::string::npos) {
                        const std::string prefix = ToUpper(command_input);
                        auto match = std::find_if(commands.begin(), commands.end(), [&prefix](const std::string& command) { return command.rfind(prefix, 0) == 0; });
                        if (match != commands.end()) command_input = *match + " ";
                    } else CompleteCommandPath();
                } else if (ch >= 32 && ch <= 126) {
                    command_input.push_back(ch);
                }
                continue;
            }

            const bool isCtrlPressed = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            if (isCtrlPressed) {
                if (vk == 'A') {
                    active_buf->insert_mode = !active_buf->insert_mode;
                    message = active_buf->insert_mode ? "Insert mode" : "Overstrike mode";
                    continue;
                } else if (vk == 'K') {
                    CutSelection();
                    continue;
                } else if (vk == 'U') {
                    PasteClipboard();
                    continue;
                } else if (vk == 'W') {
                    SaveFile(active_buf->filename);
                    continue;
                } else if (vk == 'Y') {
                    Redo();
                    continue;
                } else if (vk == 'F') {
                    in_command_mode = true;
                    command_input = "FIND ";
                    continue;
                } else if (vk == VK_TAB) {
                    IndentSelection((ctrl & SHIFT_PRESSED) != 0);
                    continue;
                } else if (vk == VK_LEFT) {
                    MoveWordLeft();
                    continue;
                } else if (vk == VK_RIGHT) {
                    MoveWordRight();
                    continue;
                } else if (vk == VK_UP) {
                    MoveParagraphUp();
                    continue;
                } else if (vk == VK_DOWN) {
                    MoveParagraphDown();
                    continue;
                } else if (vk == VK_PRIOR) {
                    if (!buffers.empty()) {
                        size_t prev_idx = (active_buffer_index > 0) ? active_buffer_index - 1 : buffers.size() - 1;
                        LoadActiveBufferState(prev_idx);
                        message = "Switched to buffer: " + active_buf->buffer_name;
                    }
                    continue;
                } else if (vk == VK_NEXT) {
                    if (!buffers.empty()) {
                        size_t next_idx = (active_buffer_index + 1 < buffers.size()) ? active_buffer_index + 1 : 0;
                        LoadActiveBufferState(next_idx);
                        message = "Switched to buffer: " + active_buf->buffer_name;
                    }
                    continue;
                }
            }

            // Text Navigation & Editing
            DispatchEditKey(ker);
        }
    }
};

static bool SafePipeWrite(const std::string& commandLine, const std::string& inputData) {
    if (commandLine.empty()) return false;

    HANDLE hChildStdInRead = NULL;
    HANDLE hChildStdInWrite = NULL;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdInRead, &hChildStdInWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdInRead);
        CloseHandle(hChildStdInWrite);
        return false;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = hChildStdInRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::vector<char> cmdBuf(commandLine.begin(), commandLine.end());
    cmdBuf.push_back('\0');

    BOOL success = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    CloseHandle(hChildStdInRead);

    if (!success) {
        CloseHandle(hChildStdInWrite);
        return false;
    }

    DWORD written = 0;
    WriteFile(hChildStdInWrite, inputData.data(), static_cast<DWORD>(inputData.size()), &written, NULL);
    CloseHandle(hChildStdInWrite);

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(piProcInfo.hProcess, &exitCode);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return exitCode == 0;
}

static int eve_main(int argc, char* argv[]) {
    std::vector<std::string> files;
    bool read_only = false;
    int format = 0;
    std::string pipeCommand;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::string upper_arg = arg;
        std::transform(upper_arg.begin(), upper_arg.end(), upper_arg.begin(), ::toupper);
        if (upper_arg == "/JSON") { format = 1; continue; }
        if (upper_arg == "/CSV") { format = 2; continue; }
        if (upper_arg == "/TABLE") { format = 3; continue; }
        if (upper_arg == "/PIPE" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (upper_arg == "/READ_ONLY" || upper_arg == "/NOWRITE" || upper_arg == "-R") {
            read_only = true;
        } else if (arg[0] != '/' && arg[0] != '-') {
            files.push_back(argv[i]);
        }
    }
    EveEditor editor(files, read_only);
    editor.Run();
    if (format || !pipeCommand.empty()) {
        std::string text = format == 1 ? "{\"status\":\"completed\",\"files\":" + std::to_string(files.size()) + "}\n" : format == 2 ? "status,files\ncompleted," + std::to_string(files.size()) + "\n" : "STATUS\tFILES\ncompleted\t" + std::to_string(files.size()) + "\n";
        if (!pipeCommand.empty()) {
            if (!SafePipeWrite(pipeCommand, text)) return 1;
        } else {
            std::cout << text;
        }
    }
    return 0;
}

class EveApplication { public: int run(int argc, char* argv[]) const { return eve_main(argc, argv); } };
int main(int argc, char* argv[]) { return EveApplication().run(argc, argv); }