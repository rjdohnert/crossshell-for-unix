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
