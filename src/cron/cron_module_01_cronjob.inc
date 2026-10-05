struct CronJob {
    std::string minutePattern;
    std::string hourPattern;
    std::string domPattern;
    std::string monthPattern;
    std::string dowPattern;
    std::string command;
    std::string rawLine;
};

