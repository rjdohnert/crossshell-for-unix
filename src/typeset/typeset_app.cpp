#include "environment_store.hpp"
#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "registry_environment_store.hpp"
#include "string_utils.hpp"
#include "typeset_app.hpp"
#include "typeset_options.hpp"
#include "typeset_pipe.hpp"
#include "variable_attributes.hpp"

int TypesetApplication::Run(int argc, wchar_t* argv[]) {
        TypesetOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            OptionParser::PrintHelp();
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        auto envMap = EnvironmentStore::GetProcessEnvironmentMap();
        FILE* pipe = opts.pipeCommand.empty() ? nullptr : _wpopen(opts.pipeCommand.c_str(), L"w");
        std::wstreambuf* oldOutput = nullptr;
        TypesetPipe* pipeBuffer = nullptr;
        if (pipe) {
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new TypesetPipe(pipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        OutputFormatter formatter(opts.format);

        // Case 1: No targets specified -> list all environment variables
        if (opts.targets.empty()) {
            for (const auto& pair : envMap) {
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(pair.first, pair.second, attr);
            }
            if (pipeBuffer) {
                std::wcout.flush();
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                _pclose(pipe);
            }
            return 0;
        }

        // Case 2: Process targets (assignments or queries)
        for (const auto& target : opts.targets) {
            std::wstring varName;
            std::wstring varVal;
            size_t eqPos = target.find(L'=');
            bool hasAssignment = (eqPos != std::wstring::npos);

            if (hasAssignment) {
                varName = target.substr(0, eqPos);
                varVal = target.substr(eqPos + 1);
            } else {
                varName = target;
                if (envMap.find(varName) != envMap.end()) {
                    varVal = envMap[varName];
                }
            }

            // Apply formatting / value transformation qualifiers
            if (hasAssignment) {
                varVal = StringUtils::ApplyAttributes(varVal, opts.attributes);
            }

            // If print mode or query mode without assignment
            if (opts.optPrint || !hasAssignment) {
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(varName, varVal, attr);
                continue;
            }

            // Apply to process environment
            if (!varName.empty()) {
                EnvironmentStore::SetProcessEnvironmentVariable(varName, varVal);
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(varName, varVal, attr);
            }

            // Persist to Windows User Registry if -g / --global specified
            if (opts.attributes.isGlobal && !varName.empty()) {
                if (RegistryEnvironmentStore::SetPersistentUserEnv(varName, varVal)) {
                    if (opts.format == OutputFormat::Default) {
                        std::wcout << L"typeset: Saved '" << varName << L"' to Windows User Registry (HKCU\\Environment)\n";
                    }
                } else {
                    std::wcerr << L"typeset: Failed to save '" << varName << L"' to Windows Registry\n";
                }
            }
        }

        if (pipeBuffer) {
            std::wcout.flush();
            std::wcout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(pipe);
        }
        return 0;
    }
