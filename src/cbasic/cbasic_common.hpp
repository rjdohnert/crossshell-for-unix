#pragma once

#include <filesystem>
#include <string>
#include <variant>

namespace fs = std::filesystem;
using Value = std::variant<double, std::string>;

extern fs::path g_virtualRoot;

bool isPathWithinRoot(const fs::path& root, const fs::path& target);
bool resolveSandboxPath(const std::string& inputPath, std::string& resolvedOut, std::string& errorOut);
std::string getSandboxRootDisplay();
double asDouble(const Value& val);
std::string valueToString(const Value& val);
bool valueToBool(const Value& val);
std::string ensureExtension(std::string filename, const std::string& defaultExt = ".bas");
void printErrorLine(const std::string& message);
void printHelp();
void printVersion();
