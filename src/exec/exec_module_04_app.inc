class ExecApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ExecOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.showHelp) {
            options.PrintUsage();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        if (options.commandIndex >= argc) {
            return 0;
        }

        DWORD exitCode = 0;
        int launchResult = ProcessLauncher::ExecuteCommand(argc, argv, options, exitCode);
        if (launchResult != 0) {
            return launchResult;
        }

        ExecReporter::ReportStatus(options.format, options.pipeCommand, exitCode);
        ExitProcess(exitCode);
    }
};
