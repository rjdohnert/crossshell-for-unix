// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class LessApp {
public:
    static int run(int argc, char* argv[]) {
        LessOptions options;
        if (!LessOptions::parse(argc, argv, options)) {
            return 1;
        }

        for (const auto& file : options.files) {
            std::vector<std::string> lines;
            if (file == "-") {
                std::string line;
                while (std::getline(std::cin, line)) lines.push_back(line);
            } else {
                std::ifstream in(file);
                if (!in.is_open()) {
                    std::cerr << "less: " << file << ": No such file or directory\n";
                    continue;
                }
                std::string line;
                while (std::getline(in, line)) lines.push_back(line);
            }

            if (!_isatty(_fileno(stdout))) {
                for (const auto& l : lines) std::cout << l << "\n";
                continue;
            }

            TextPagerEngine pager(lines, file, options);
            pager.run();
        }

        return 0;
    }
};

