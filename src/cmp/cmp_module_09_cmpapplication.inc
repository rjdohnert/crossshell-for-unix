    class CmpApplication {
    public:
        static int execute(int argc, char* argv[]) {
            // Speed up C++ stream I/O
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            Options opts;
            if (!ArgParser::parse(argc, argv, opts)) {
                return static_cast<int>(ExitCode::Error);
            }

            if (opts.show_help) {
                HelpSystem::print_help();
                return static_cast<int>(ExitCode::Identical);
            }

            if (opts.show_version) {
                HelpSystem::print_version();
                return static_cast<int>(ExitCode::Identical);
            }

            FileComparator comparator(opts);
            return static_cast<int>(comparator.run());
        }
    };
}

int main(int argc, char* argv[]) {
    return CmpUtil::CmpApplication::execute(argc, argv);
}