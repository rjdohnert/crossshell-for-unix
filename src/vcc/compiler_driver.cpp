#include "build_options.hpp"
#include "command_line_formatter.hpp"
#include "compile_logger.hpp"
#include "compiler_driver.hpp"
#include "flag_translator.hpp"

int CompilerDriver::Run(const BuildOptions& opts) {
        if (opts.showHelp) {
            FlagTranslator::PrintHelp(opts.programName);
            return 0;
        }
        if (opts.showVersion) {
            FlagTranslator::PrintVersion();
            return 0;
        }

        bool clInPath = m_locator.IsClInPath();
        std::ostringstream cmd;

        if (clInPath) {
            cmd << "cl.exe";
        } else {
            std::string vsPath = m_locator.FindVsInstallation();
            if (vsPath.empty()) {
                std::cerr << "[vcc] Error: Visual Studio could not be found via vswhere.exe.\n";
                return 1;
            }
            std::string vcvars = m_locator.GetVcvarsBatchPath(vsPath, opts.targetArch);
            cmd << "cmd.exe /S /C \"call \"" << vcvars << "\" >nul 2>&1 && cl.exe";
        }

        for (const auto& cArg : opts.compilerArgs) {
            cmd << " " << CommandLineFormatter::Quote(cArg);
        }

        if (!opts.linkerArgs.empty()) {
            cmd << " /link";
            for (const auto& lArg : opts.linkerArgs) {
                cmd << " " << CommandLineFormatter::Quote(lArg);
            }
        }

        if (!clInPath) {
            cmd << "\"";
        }

        CompileLogger::LogInvocation(cmd.str());
        int result = system(cmd.str().c_str());
        CompileLogger::LogResult(result);
        return result;
    }
