#pragma once

#include "dependency_spec.hpp"
#include "file_spec.hpp"
#include "rpmbuild.hpp"

class SpecParser {
public:
    std::map<std::string, std::string> macros;
    std::string name;
    std::string version;
    std::string release = "1";
    std::string epoch = "0";
    std::string summary;
    std::string description;
    std::string license = "Proprietary";
    std::string group = "Applications/System";
    std::string url;
    std::string vendor = "Enterprise Deployment";
    std::string packager = "RPM Build Engine";
    std::string buildArch = "x86_64";
    std::string buildRoot;

    std::string prepScript;
    std::string buildScript;
    std::string installScript;
    std::string cleanScript;
    std::string preScript;
    std::string postScript;
    std::string preunScript;
    std::string postunScript;

    std::vector<FileSpec> files;
    std::vector<DependencySpec> requirements;
    std::vector<DependencySpec> provides;
    std::vector<DependencySpec> conflicts;

    SpecParser();

    static std::string stripQuotes(const std::string& str);

    std::string expand(const std::string& input);

    std::string getMacroValue(const std::string& key);

    bool parseFile(const fs::path& specPath);

private:
    void parseDependencyList(const std::string& line, std::vector<DependencySpec>& list);

    void parseFileDirective(const std::string& line);
};
