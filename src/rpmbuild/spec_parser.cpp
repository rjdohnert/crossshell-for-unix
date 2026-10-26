#include "dependency_spec.hpp"
#include "file_spec.hpp"
#include "rpm_format.hpp"
#include "spec_parser.hpp"

SpecParser::SpecParser() {
        // Built-in standard directory macros adapted for Windows
        macros["_prefix"]        = "C:\\Program Files";
        macros["_bindir"]        = "C:\\Program Files\\%{name}\\bin";
        macros["_sysconfdir"]    = "C:\\ProgramData\\%{name}\\config";
        macros["_datadir"]       = "C:\\Program Files\\%{name}\\share";
        macros["_defaultdocdir"] = "C:\\Program Files\\%{name}\\doc";
        macros["_tmppath"]       = fs::temp_directory_path().string();
    }

std::string SpecParser::stripQuotes(const std::string& str) {
        std::string s = str;
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        s = s.substr(first, (last - first + 1));
        if (s.size() >= 2) {
            if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
                s = s.substr(1, s.size() - 2);
            }
        }
        return s;
    }

std::string SpecParser::expandBracedOnly(const std::string& input) {
        std::string result = input;
        int depth = 0;
        while (depth++ < 15) {
            bool matched = false;
            std::regex bracedRegex(R"(%\{([a-zA-Z0-9_]+)\})");
            std::smatch match;
            if (std::regex_search(result, match, bracedRegex)) {
                std::string key = match[1].str();
                std::string val = getMacroValue(key);
                result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                matched = true;
            }
            if (!matched) break;
        }
        return result;
    }

std::string SpecParser::expand(const std::string& input) {
        std::string result = input;
        int depth = 0;

        while (depth++ < 15) {
            bool matched = false;

            // 1. Match %{macro_name}
            std::regex bracedRegex(R"(%\{([a-zA-Z0-9_]+)\})");
            std::smatch match;
            if (std::regex_search(result, match, bracedRegex)) {
                std::string key = match[1].str();
                std::string val = getMacroValue(key);
                result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                matched = true;
            }

            // 2. Match %macro_name (without braces) - only if NOT part of Windows batch %VAR%
            if (!matched) {
                std::regex simpleRegex(R"(%([a-zA-Z0-9_]+))");
                if (std::regex_search(result, match, simpleRegex)) {
                    size_t afterMatch = match.position(0) + match.length(0);
                    if (afterMatch < result.size() && result[afterMatch] == '%') {
                        // Skip Windows environment variables like %BUILDROOT%
                    } else {
                        std::string key = match[1].str();
                        if (key != "prep" && key != "build" && key != "install" && key != "clean" &&
                            key != "pre" && key != "post" && key != "preun" && key != "postun" &&
                            key != "files" && key != "description" && key != "package" &&
                            key != "setup" && key != "autosetup" && key != "patch") {
                            std::string val = getMacroValue(key);
                            if (!val.empty() || macros.count(key) > 0) {
                                result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                                matched = true;
                            }
                        }
                    }
                }
            }

            if (!matched) break;
        }
        return result;
    }

std::string SpecParser::getMacroValue(const std::string& key) {
        std::string lowerKey = key;
        for (char& c : lowerKey) c = (char)tolower((unsigned char)c);

        if (lowerKey == "name") return !name.empty() ? name : (macros.count("name") ? macros["name"] : "");
        if (lowerKey == "version") return !version.empty() ? version : (macros.count("version") ? macros["version"] : "");
        if (lowerKey == "release") return !release.empty() ? release : (macros.count("release") ? macros["release"] : "");
        if (lowerKey == "epoch") return !epoch.empty() ? epoch : (macros.count("epoch") ? macros["epoch"] : "0");
        if (lowerKey == "buildroot" || lowerKey == "build_root") return !buildRoot.empty() ? buildRoot : (macros.count("buildroot") ? macros["buildroot"] : "");
        if (lowerKey == "_tmppath") return fs::temp_directory_path().string();

        auto it = macros.find(key);
        if (it != macros.end()) return it->second;
        auto itLower = macros.find(lowerKey);
        if (itLower != macros.end()) return itLower->second;

        return "";
    }

bool SpecParser::parseFile(const fs::path& specPath) {
        std::ifstream file(specPath);
        if (!file.is_open()) {
            std::cerr << "error: Cannot open spec file: " << specPath.string() << "\n";
            return false;
        }

        std::string line;
        std::string currentSection = "HEADER";
        std::stringstream sectionBuffer;

        auto flushSection = [&]() {
            std::string content = sectionBuffer.str();
            sectionBuffer.str("");
            sectionBuffer.clear();
            std::string expanded = expandBracedOnly(content);
            if (currentSection == "%prep") prepScript = expanded;
            else if (currentSection == "%build") buildScript = expanded;
            else if (currentSection == "%install") installScript = expanded;
            else if (currentSection == "%clean") cleanScript = expanded;
            else if (currentSection == "%pre") preScript = expanded;
            else if (currentSection == "%post") postScript = expanded;
            else if (currentSection == "%preun") preunScript = expanded;
            else if (currentSection == "%postun") postunScript = expanded;
            else if (currentSection == "%description") description = expanded;
        };

        bool firstLine = true;
        while (std::getline(file, line)) {
            if (firstLine) {
                firstLine = false;
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                    line = line.substr(3);
                }
            }
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = line.find_last_not_of(" \t\r\n");
            std::string trimmed = line.substr(first, (last - first + 1));

            if (trimmed.rfind("#", 0) == 0) continue; // Comments

            if (trimmed.rfind("%define", 0) == 0 || trimmed.rfind("%global", 0) == 0) {
                std::istringstream iss(trimmed);
                std::string directive, key, val;
                iss >> directive >> key;
                std::getline(iss, val);
                size_t vFirst = val.find_first_not_of(" \t");
                if (vFirst != std::string::npos) val = val.substr(vFirst);
                val = stripQuotes(val);
                bool isCliDefined = false;
                for (const auto& m : cliDefinedMacros) {
                    if (_stricmp(m.c_str(), key.c_str()) == 0) {
                        isCliDefined = true;
                        break;
                    }
                }
                if (!isCliDefined) {
                    macros[key] = val;
                    if (key == "name") name = val;
                    else if (key == "version") version = val;
                    else if (key == "release") release = val;
                }
                continue;
            }

            auto isSectionHeader = [](const std::string& token) {
                return token == "%prep" || token == "%build" || token == "%install" ||
                       token == "%clean" || token == "%pre" || token == "%post" ||
                       token == "%preun" || token == "%postun" || token == "%files" ||
                       token == "%description" || token == "%package" || token == "%changelog";
            };

            if (trimmed[0] == '%') {
                std::istringstream iss(trimmed);
                std::string firstWord;
                iss >> firstWord;
                if (isSectionHeader(firstWord)) {
                    flushSection();
                    currentSection = firstWord;
                    continue;
                }
            }

            if (currentSection == "HEADER") {
                auto sep = trimmed.find(':');
                if (sep != std::string::npos) {
                    std::string key = trimmed.substr(0, sep);
                    std::string val = trimmed.substr(sep + 1);
                    size_t vFirst = val.find_first_not_of(" \t");
                    if (vFirst != std::string::npos) val = val.substr(vFirst);
                    val = expand(stripQuotes(val));

                    std::string lowerKey = key;
                    for (char& c : lowerKey) c = (char)tolower((unsigned char)c);

                    auto isCli = [&](const std::string& k) {
                        for (const auto& m : cliDefinedMacros) {
                            if (_stricmp(m.c_str(), k.c_str()) == 0) return true;
                        }
                        return false;
                    };

                    if (lowerKey == "name") { if (!isCli("name")) { name = val; macros["name"] = val; } }
                    else if (lowerKey == "version") { if (!isCli("version")) { version = val; macros["version"] = val; } }
                    else if (lowerKey == "release") { if (!isCli("release")) { release = val; macros["release"] = val; } }
                    else if (lowerKey == "epoch") { epoch = val; macros["epoch"] = val; }
                    else if (lowerKey == "summary") summary = val;
                    else if (lowerKey == "license") license = val;
                    else if (lowerKey == "group") group = val;
                    else if (lowerKey == "url") url = val;
                    else if (lowerKey == "vendor") vendor = val;
                    else if (lowerKey == "packager") packager = val;
                    else if (lowerKey == "buildarch") buildArch = val;
                    else if (lowerKey == "buildroot") buildRoot = val;
                    else if (lowerKey.rfind("source", 0) == 0) sources.push_back(val);
                    else if (lowerKey.rfind("patch", 0) == 0) patches.push_back(val);
                    else if (lowerKey == "requires") parseDependencyList(val, requirements);
                    else if (lowerKey == "provides") parseDependencyList(val, provides);
                    else if (lowerKey == "conflicts") parseDependencyList(val, conflicts);
                }
            } else if (currentSection == "%files") {
                parseFileDirective(trimmed);
            } else {
                sectionBuffer << line << "\n";
            }
        }
        flushSection();

        if (buildRoot.empty()) {
            buildRoot = (fs::temp_directory_path() / ("BUILDROOT_" + name + "-" + version + "-" + release)).string();
        }
        buildRoot = expand(buildRoot);
        return true;
    }

void SpecParser::parseDependencyList(const std::string& line, std::vector<DependencySpec>& list) {
        std::vector<std::string> tokens;
        std::string current;
        for (char c : line) {
            if (c == ',' || isspace((unsigned char)c)) {
                if (!current.empty()) {
                    tokens.push_back(stripQuotes(current));
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) tokens.push_back(stripQuotes(current));

        auto isOp = [](const std::string& s) {
            return s == ">=" || s == "<=" || s == "=" || s == "==" || s == ">" || s == "<";
        };

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].empty()) continue;
            DependencySpec spec;
            spec.name = tokens[i];
            if (i + 1 < tokens.size() && isOp(tokens[i + 1])) {
                std::string op = tokens[++i];
                if (i + 1 < tokens.size()) {
                    spec.version = tokens[++i];
                    if (op == ">=") spec.flags = RPMSENSE_GREATER | RPMSENSE_EQUAL;
                    else if (op == "<=") spec.flags = RPMSENSE_LESS | RPMSENSE_EQUAL;
                    else if (op == "=" || op == "==") spec.flags = RPMSENSE_EQUAL;
                    else if (op == ">") spec.flags = RPMSENSE_GREATER;
                    else if (op == "<") spec.flags = RPMSENSE_LESS;
                }
            }
            list.push_back(spec);
        }
    }

void SpecParser::parseFileDirective(const std::string& line) {
        FileSpec spec;
        std::string pathPart = line;
        bool customModeSet = false;

        // Parse %attr(mode, user, group)
        if (pathPart.rfind("%attr(", 0) == 0) {
            auto closeParen = pathPart.find(')');
            if (closeParen != std::string::npos) {
                std::string attrBody = pathPart.substr(6, closeParen - 6);
                pathPart = pathPart.substr(closeParen + 1);

                std::stringstream attrSS(attrBody);
                std::string modeStr;
                if (std::getline(attrSS, modeStr, ',')) {
                    size_t mFirst = modeStr.find_first_not_of(" \t-");
                    if (mFirst != std::string::npos) {
                        try {
                            uint32_t oct = static_cast<uint32_t>(std::stoul(modeStr.substr(mFirst), nullptr, 8));
                            if (oct > 0) {
                                spec.mode = (oct >= 0100000) ? oct : (0100000 | oct);
                                customModeSet = true;
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        // Parse %config and %doc directives
        if (pathPart.rfind("%config(noreplace)", 0) == 0) {
            spec.flags |= (RPMFILE_CONFIG | RPMFILE_NOREPLACE);
            pathPart = pathPart.substr(18);
        } else if (pathPart.rfind("%config", 0) == 0) {
            spec.flags |= RPMFILE_CONFIG;
            pathPart = pathPart.substr(7);
        } else if (pathPart.rfind("%doc", 0) == 0) {
            spec.flags |= RPMFILE_DOC;
            pathPart = pathPart.substr(4);
        } else if (pathPart.rfind("%dir", 0) == 0) {
            pathPart = pathPart.substr(4);
        }

        pathPart = stripQuotes(pathPart);
        spec.path = expand(pathPart);

        if (!customModeSet) {
            spec.mode = (spec.path.find(".exe") != std::string::npos ||
                         spec.path.find(".bat") != std::string::npos ||
                         spec.path.find(".cmd") != std::string::npos ||
                         spec.path.find(".ps1") != std::string::npos) ? 0100755 : 0100644;
        }
        files.push_back(spec);
    }
