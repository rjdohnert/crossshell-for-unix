struct SplitPattern {
    PatternType type{PatternType::LineNum};
    long long lineNum{0};
    std::string regexStr;
    long long offset{0};
    int repeat{0}; // 0 = run 1 time; N = repeat N times; -1 = {*}
};

