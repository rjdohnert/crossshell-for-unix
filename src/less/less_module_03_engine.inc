// ============================================================================
// 3. INTERACTIVE PAGER ENGINE
// ============================================================================

class TextPagerEngine {
private:
    std::vector<std::string> lines;
    std::string filename;
    LessOptions options;
    int topIndex{0};
    std::string searchQuery;

    static std::string toLower(const std::string& str) {
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
        return res;
    }

    static std::string expandTabs(const std::string& input, int tabWidth = 4) {
        std::string result;
        for (char c : input) {
            if (c == '\t') {
                int spaces = tabWidth - (static_cast<int>(result.length()) % tabWidth);
                result.append(spaces, ' ');
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    void render() {
        int width = 80, height = 25;
        TerminalDisplay::getSize(width, height);
        int viewHeight = (std::max)(1, height - 1);

        std::string buffer;
        buffer += "\033[H\033[2J"; // Clear screen

        for (int i = 0; i < viewHeight; ++i) {
            int lineIdx = topIndex + i;
            if (lineIdx < static_cast<int>(lines.size())) {
                std::string line = lines[lineIdx];
                line = expandTabs(line);

                if (options.showLineNumbers) {
                    char numBuf[32];
                    snprintf(numBuf, sizeof(numBuf), "\033[33m%6d\033[0m ", lineIdx + 1);
                    buffer += numBuf;
                }

                if (!searchQuery.empty()) {
                    std::string lowerLine = toLower(line);
                    std::string lowerQuery = toLower(searchQuery);
                    size_t pos = 0;
                    std::string highlighted;
                    while (pos < line.size()) {
                        size_t match = lowerLine.find(lowerQuery, pos);
                        if (match == std::string::npos) {
                            highlighted += line.substr(pos);
                            break;
                        }
                        highlighted += line.substr(pos, match - pos);
                        highlighted += "\033[7m" + line.substr(match, searchQuery.size()) + "\033[0m";
                        pos = match + searchQuery.size();
                    }
                    line = highlighted;
                }

                buffer += line;
            } else {
                buffer += "~";
            }
            buffer += "\033[K\n"; // Clear to end of line
        }

        // Status bar
        buffer += "\033[7m";
        if (lines.empty()) {
            buffer += " (END) - " + filename;
        } else {
            int pct = (lines.size() <= static_cast<size_t>(viewHeight)) ? 100
                    : static_cast<int>((static_cast<double>(topIndex) / (lines.size() - viewHeight)) * 100.0);
            pct = (std::min)(100, (std::max)(0, pct));
            buffer += " " + filename + " lines " + std::to_string(topIndex + 1) + "-"
                   + std::to_string((std::min)(static_cast<int>(lines.size()), topIndex + viewHeight))
                   + "/" + std::to_string(lines.size()) + " (" + std::to_string(pct) + "%)";
        }
        buffer += "\033[0m";

        std::cout << buffer << std::flush;
    }

public:
    TextPagerEngine(std::vector<std::string> l, std::string fn, LessOptions opts)
        : lines(std::move(l)), filename(std::move(fn)), options(std::move(opts)) {}

    void run() {
        TerminalDisplay::enableVtMode();

        int width = 80, height = 25;
        TerminalDisplay::getSize(width, height);
        int viewHeight = (std::max)(1, height - 1);

        render();

        while (true) {
            KeyCode key = TerminalDisplay::readKey();
            TerminalDisplay::getSize(width, height);
            viewHeight = (std::max)(1, height - 1);
            int maxTop = (std::max)(0, static_cast<int>(lines.size()) - viewHeight);

            if (key == KeyCode::QUIT) break;
            else if (key == KeyCode::DOWN) {
                if (topIndex < maxTop) topIndex++;
            } else if (key == KeyCode::UP) {
                if (topIndex > 0) topIndex--;
            } else if (key == KeyCode::PAGE_DOWN) {
                topIndex = (std::min)(maxTop, topIndex + viewHeight);
            } else if (key == KeyCode::PAGE_UP) {
                topIndex = (std::max)(0, topIndex - viewHeight);
            } else if (key == KeyCode::HOME) {
                topIndex = 0;
            } else if (key == KeyCode::END) {
                topIndex = maxTop;
            } else if (key == KeyCode::SEARCH_FWD) {
                std::cout << "\033[" << height << ";1H\033[K/";
                std::string q;
                std::getline(std::cin, q);
                if (!q.empty()) searchQuery = q;
            }

            render();
        }

        std::cout << "\033[H\033[2J\033[0m" << std::flush;
    }
};

