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

            // 2. Match %macro_name (without braces)
            if (!matched) {
                std::regex simpleRegex(R"(%([a-zA-Z0-9_]+))");
                if (std::regex_search(result, match, simpleRegex)) {
                    std::string key = match[1].str();
                    // Avoid expanding directives like %prep, %build, %files etc. if encountered in code
                    if (key != "prep" && key != "build" && key != "install" && key != "clean" &&
                        key != "pre" && key != "post" && key != "preun" && key != "postun" &&
                        key != "files" && key != "description" && key != "package") {
                        std::string val = getMacroValue(key);
                        result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                        matched = true;
                    }
                }
            }

            if (!matched) break;
        }
        return result;
    }

std::string SpecParser::getMacroValue(const std::string& key) {
        if (key == "name") return !name.empty() ? name : (macros.count("name") ? macros["name"] : "");
        if (key == "version") return !version.empty() ? version : (macros.count("version") ? macros["version"] : "");
        if (key == "release") return !release.empty() ? release : (macros.count("release") ? macros["release"] : "");
        if (key == "epoch") return !epoch.empty() ? epoch : (macros.count("epoch") ? macros["epoch"] : "0");
        if (key == "buildroot" || key == "build_root") return !buildRoot.empty() ? buildRoot : (macros.count("buildroot") ? macros["buildroot"] : "");
        if (macros.find(key) != macros.end()) return macros[key];
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
            if (currentSection == "%prep") prepScript = content;
            else if (currentSection == "%build") buildScript = content;
            else if (currentSection == "%install") installScript = content;
            else if (currentSection == "%clean") cleanScript = content;
            else if (currentSection == "%pre") preScript = content;
            else if (currentSection == "%post") postScript = content;
            else if (currentSection == "%preun") preunScript = content;
            else if (currentSection == "%postun") postunScript = content;
            else if (currentSection == "%description") description = content;
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
                macros[key] = val;
                if (key == "name") name = val;
                else if (key == "version") version = val;
                else if (key == "release") release = val;
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
                    val = stripQuotes(val);

                    if (_stricmp(key.c_str(), "Name") == 0) { name = val; macros["name"] = val; }
                    else if (_stricmp(key.c_str(), "Version") == 0) { version = val; macros["version"] = val; }
                    else if (_stricmp(key.c_str(), "Release") == 0) { release = val; macros["release"] = val; }
                    else if (_stricmp(key.c_str(), "Epoch") == 0) { epoch = val; macros["epoch"] = val; }
                    else if (_stricmp(key.c_str(), "Summary") == 0) summary = val;
                    else if (_stricmp(key.c_str(), "License") == 0) license = val;
                    else if (_stricmp(key.c_str(), "Group") == 0) group = val;
                    else if (_stricmp(key.c_str(), "URL") == 0) url = val;
                    else if (_stricmp(key.c_str(), "Vendor") == 0) vendor = val;
                    else if (_stricmp(key.c_str(), "BuildArch") == 0) buildArch = val;
                    else if (_stricmp(key.c_str(), "BuildRoot") == 0) buildRoot = val;
                    else if (_stricmp(key.c_str(), "Requires") == 0) parseDependencyList(val, requirements);
                    else if (_stricmp(key.c_str(), "Provides") == 0) parseDependencyList(val, provides);
                    else if (_stricmp(key.c_str(), "Conflicts") == 0) parseDependencyList(val, conflicts);
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
        // Split dependencies by comma
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::istringstream iss(item);
            std::string depName, op, depVer;
            if (iss >> depName) {
                DependencySpec spec;
                spec.name = stripQuotes(depName);
                if (iss >> op >> depVer) {
                    spec.version = stripQuotes(depVer);
                    if (op == ">=") spec.flags = RPMSENSE_GREATER | RPMSENSE_EQUAL;
                    else if (op == "<=") spec.flags = RPMSENSE_LESS | RPMSENSE_EQUAL;
                    else if (op == "=" || op == "==") spec.flags = RPMSENSE_EQUAL;
                    else if (op == ">") spec.flags = RPMSENSE_GREATER;
                    else if (op == "<") spec.flags = RPMSENSE_LESS;
                }
                list.push_back(spec);
            }
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
