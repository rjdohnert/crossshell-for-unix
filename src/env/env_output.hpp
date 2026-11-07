#ifndef ENV_OUTPUT_HPP
#define ENV_OUTPUT_HPP

#include "env_environment.hpp"
#include "env_options.hpp"

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

using TextSink = std::function<bool(std::wstring_view)>;

bool WriteEnvironment(const EnvironmentManager::Map& environment,
                      OutputFormat format, bool nullDelimiter,
                      const TextSink& sink, std::wstring& error);
void PrintHelp(std::wostream& output);
void PrintVersion(std::wostream& output);

#endif
