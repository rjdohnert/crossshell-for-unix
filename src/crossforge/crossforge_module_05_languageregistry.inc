class LanguageRegistry {
private:
    std::map<SupportedLanguage, LanguageConfig> m_configs;

public:
    LanguageRegistry() {
        InitLanguages();
        const char* modernCppKeywords[] = {
            "alignof", "char8_t", "char16_t", "char32_t", "nullptr", "static_assert",
            "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast", "thread_local",
            "concept", "requires", "co_await", "co_return", "co_yield"
        };
        for (const char* keyword : modernCppKeywords) {
            m_configs[SupportedLanguage::Cpp].Keywords.push_back(keyword);
        }
    }

    void InitLanguages() {
        // 1. C++
        m_configs[SupportedLanguage::Cpp] = {
            SupportedLanguage::Cpp, "C++", {".cpp", ".cxx", ".cc", ".h", ".hpp"},
            {"alignas", "auto", "bool", "break", "case", "catch", "class", "const", "constexpr",
             "continue", "default", "delete", "do", "double", "else", "enum", "explicit", "export",
             "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
             "namespace", "new", "operator", "private", "protected", "public", "return", "short",
             "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true",
             "try", "typedef", "typename", "union", "unsigned", "using", "virtual", "void", "while"},
            "cl.exe", "cdb.exe", "cl.exe /std:c++17 /EHsc \"%f\"", "cdb.exe -g \"%e\""
        };

        // 2. C
        m_configs[SupportedLanguage::C] = {
            SupportedLanguage::C, "C", {".c", ".h"},
            {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
             "enum", "extern", "float", "for", "goto", "if", "int", "long", "register", "return",
             "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"},
            "gcc.exe", "gdb.exe", "gcc \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 3. Java
        m_configs[SupportedLanguage::Java] = {
            SupportedLanguage::Java, "Java", {".java"},
            {"abstract", "boolean", "break", "byte", "case", "catch", "char", "class", "continue",
             "default", "do", "double", "else", "extends", "final", "finally", "float", "for", "if",
             "implements", "import", "instanceof", "int", "interface", "long", "native", "new", "package",
             "private", "protected", "public", "return", "short", "static", "super", "switch", "synchronized",
             "this", "throw", "throws", "transient", "try", "void", "volatile", "while"},
            "javac.exe", "jdb.exe", "javac \"%f\"", "jdb \"%e\""
        };

        // 4. C#
        m_configs[SupportedLanguage::CSharp] = {
            SupportedLanguage::CSharp, "C#", {".cs"},
            {"abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "checked",
             "class", "const", "continue", "decimal", "default", "delegate", "do", "double", "else",
             "enum", "event", "explicit", "extern", "false", "finally", "fixed", "float", "for",
             "foreach", "goto", "if", "implicit", "in", "int", "interface", "internal", "is", "lock",
             "long", "namespace", "new", "null", "object", "operator", "out", "override", "params",
             "private", "protected", "public", "readonly", "ref", "return", "sbyte", "sealed", "short",
             "sizeof", "stackalloc", "static", "string", "struct", "switch", "this", "throw", "true",
             "try", "typeof", "uint", "ulong", "unchecked", "unsafe", "ushort", "using", "virtual", "void", "while"},
            "csc.exe", "vsdbg.exe", "csc /target:exe \"%f\"", "vsdbg --engine \"%e.exe\""
        };

        // 5. Objective-C
        m_configs[SupportedLanguage::ObjC] = {
            SupportedLanguage::ObjC, "Objective-C", {".m", ".mm"},
            {"@interface", "@implementation", "@end", "@property", "@synthesize", "@selector", "@protocol",
             "id", "Class", "SEL", "BOOL", "YES", "NO", "nil", "Nil", "self", "super", "alloc", "init"},
            "clang.exe", "lldb.exe", "clang -framework Foundation \"%f\" -o \"%e.exe\"", "lldb \"%e.exe\""
        };

        // 6. Swift
        m_configs[SupportedLanguage::Swift] = {
            SupportedLanguage::Swift, "Swift", {".swift"},
            {"class", "deinit", "enum", "extension", "func", "import", "init", "inout", "internal", "let",
             "operator", "private", "protocol", "public", "static", "struct", "subscript", "typealias", "var",
             "break", "case", "continue", "default", "defer", "do", "else", "fallthrough", "for", "guard", "if",
             "in", "repeat", "return", "switch", "where", "while", "as", "catch", "false", "is", "nil", "rethrows",
             "super", "self", "Self", "throw", "true", "try"},
            "swiftc.exe", "lldb.exe", "swiftc \"%f\" -o \"%e.exe\"", "lldb \"%e.exe\""
        };

        // 7. TypeScript
        m_configs[SupportedLanguage::TypeScript] = {
            SupportedLanguage::TypeScript, "TypeScript", {".ts", ".tsx"},
            {"any", "boolean", "constructor", "declare", "get", "instanceof", "interface", "enum", "type",
             "number", "private", "protected", "public", "readonly", "require", "set", "string", "symbol",
             "module", "namespace", "abstract", "as", "implements", "is", "let", "const", "var", "function", "import", "export"},
            "tsc.cmd", "node.exe", "tsc \"%f\"", "node --inspect-brk \"%e.js\""
        };

        // 8. JavaScript
        m_configs[SupportedLanguage::JavaScript] = {
            SupportedLanguage::JavaScript, "JavaScript", {".js", ".jsx"},
            {"async", "await", "break", "case", "catch", "class", "const", "continue", "debugger", "default",
             "delete", "do", "else", "export", "extends", "false", "finally", "for", "function", "if",
             "import", "in", "instanceof", "new", "null", "return", "super", "switch", "this", "throw",
             "true", "try", "typeof", "var", "void", "while", "with", "yield", "let"},
            "node.exe", "node.exe", "node \"%f\"", "node --inspect-brk \"%f\""
        };

        // 9. Node.js
        m_configs[SupportedLanguage::NodeJS] = {
            SupportedLanguage::NodeJS, "Node.js", {".cjs", ".mjs"},
            {"require", "exports", "module", "__dirname", "__filename", "process", "Buffer", "global", "async", "await"},
            "node.exe", "node.exe", "node \"%f\"", "node --inspect-brk \"%f\""
        };

        // 10. Pascal
        m_configs[SupportedLanguage::Pascal] = {
            SupportedLanguage::Pascal, "Pascal", {".pas", ".pp", ".inc"},
            {"absolute", "and", "array", "begin", "case", "const", "constructor", "destructor", "div",
             "do", "downto", "else", "end", "file", "for", "function", "goto", "if", "implementation",
             "in", "inherited", "inline", "interface", "label", "mod", "nil", "not", "object", "of", "or",
             "packed", "procedure", "program", "record", "repeat", "set", "string", "then", "to", "type",
             "unit", "until", "uses", "var", "while", "with"},
            "fpc.exe", "gdb.exe", "fpc \"%f\"", "gdb \"%e.exe\""
        };

        // 11. Fortran
        m_configs[SupportedLanguage::Fortran] = {
            SupportedLanguage::Fortran, "Fortran", {".f", ".f90", ".f95", ".for"},
            {"program", "end", "subroutine", "function", "integer", "real", "double", "precision", "complex",
             "logical", "character", "dimension", "intent", "implicit", "none", "use", "module", "if", "then",
             "else", "endif", "do", "enddo", "while", "call", "return", "print", "write", "read"},
            "gfortran.exe", "gdb.exe", "gfortran \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 12. COBOL
        m_configs[SupportedLanguage::Cobol] = {
            SupportedLanguage::Cobol, "COBOL", {".cob", ".cbl"},
            {"IDENTIFICATION", "DIVISION", "PROGRAM-ID", "ENVIRONMENT", "DATA", "WORKING-STORAGE",
             "PROCEDURE", "DISPLAY", "ACCEPT", "STOP", "RUN", "PERFORM", "UNTIL", "VARYING", "IF", "ELSE",
             "END-IF", "MOVE", "TO", "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "COMPUTE", "PIC", "VALUE"},
            "cobc.exe", "gdb.exe", "cobc -x \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 13. Rust
        m_configs[SupportedLanguage::Rust] = {
            SupportedLanguage::Rust, "Rust", {".rs"},
            {"as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
             "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
             "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct", "super",
             "trait", "true", "type", "unsafe", "use", "where", "while", "abstract", "become",
             "box", "do", "final", "macro", "override", "priv", "typeof", "unsized", "virtual", "yield"},
            "rustc.exe", "gdb.exe", "rustc \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 14. Go
        m_configs[SupportedLanguage::Go] = {
            SupportedLanguage::Go, "Go", {".go"},
            {"break", "default", "func", "interface", "select", "case", "defer", "go", "map", "struct",
             "chan", "else", "goto", "package", "switch", "const", "fallthrough", "if", "range", "type",
             "continue", "for", "import", "return", "var", "true", "false", "nil"},
            "go.exe", "gdb.exe", "go build -o \"%e.exe\" \"%f\"", "gdb \"%e.exe\""
        };
    }

    SupportedLanguage DetectLanguage(const std::string& filepath) {
        std::string ext = fs::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        for (const auto& [lang, cfg] : m_configs) {
            for (const auto& e : cfg.Extensions) {
                if (e == ext) return lang;
            }
        }
        return SupportedLanguage::Text;
    }

    const LanguageConfig& GetConfig(SupportedLanguage lang) const {
        static LanguageConfig defaultConfig{SupportedLanguage::Text, "Plain Text", {}, {}, "", "", "", ""};
        auto it = m_configs.find(lang);
        if (it != m_configs.end()) return it->second;
        return defaultConfig;
    }
};

// ============================================================================
// COMPILER & TOOLCHAIN AUTODETECTION
// ============================================================================

