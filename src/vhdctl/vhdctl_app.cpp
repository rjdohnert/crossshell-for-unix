#include "scoped_wide_pipe_redirect.hpp"
#include "vhd_operations.hpp"
#include "vhd_reporter.hpp"
#include "vhd_storage_inspector.hpp"
#include "vhdctl_app.hpp"
#include "vhdctl_options.hpp"

int VhdctlApplication::Run(int argc, wchar_t* argv[]) const {
        VhdctlOptions opts;
        if (!opts.Parse(argc, argv)) {
            if (opts.showHelp) {
                VhdReporter::PrintUsage();
                return 0;
            }
            if (opts.showVersion) {
                VhdReporter::PrintVersion();
                return 0;
            }
            return 1;
        }

        if (opts.showHelp) {
            VhdReporter::PrintUsage();
            return 0;
        }
        if (opts.showVersion) {
            VhdReporter::PrintVersion();
            return 0;
        }

        if (!VhdStorageInspector::IsAdmin()) {
            std::wcerr << L"vhdctl: Access denied. Run this command from an elevated (Run as administrator) terminal." << std::endl;
            return ERROR_ACCESS_DENIED;
        }

        ScopedWidePipeRedirect pipeRedirect(opts.pipeCommand);
        if (!opts.pipeCommand.empty() && !pipeRedirect.IsValid()) {
            std::wcerr << L"vhdctl: failed to start pipe command.\n";
            return 1;
        }

        if (opts.command == L"create" || opts.command == L"cr") {
            if (opts.sizeStr.empty()) {
                std::wcerr << L"vhdctl: Missing required parameter -s / --size for create command." << std::endl;
                return 1;
            }
            ULONGLONG sizeBytes = VhdStorageInspector::ParseSize(opts.sizeStr);
            if (sizeBytes == 0) {
                std::wcerr << L"vhdctl: Invalid size specifier '" << opts.sizeStr << L"'." << std::endl;
                return 1;
            }
            return VhdOperations::CreateDisk(opts.filePath, sizeBytes, opts.isFixed, opts.outputFormat) ? 0 : 1;

        } else if (opts.command == L"mount" || opts.command == L"attach") {
            return VhdOperations::MountDisk(opts.filePath, opts.readOnly, opts.outputFormat) ? 0 : 1;

        } else if (opts.command == L"unmount" || opts.command == L"detach") {
            return VhdOperations::UnmountDisk(opts.filePath, opts.outputFormat) ? 0 : 1;

        } else {
            std::wcerr << L"vhdctl: Unknown command '" << opts.command << L"'." << std::endl;
            VhdReporter::PrintUsage();
            return 1;
        }
    }
