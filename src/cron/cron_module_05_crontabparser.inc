class CrontabParser {
public:
    static bool parseLine(const std::string& line, CronJob& job) {
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return false;
        trimmed = trimmed.substr(start);

        if (trimmed.empty() || trimmed[0] == '#') return false;

        std::istringstream iss(trimmed);
        if (!(iss >> job.minutePattern >> job.hourPattern >> job.domPattern >> job.monthPattern >> job.dowPattern)) {
            return false;
        }

        std::string remainder;
        std::getline(iss, remainder);
        size_t cmdStart = remainder.find_first_not_of(" \t");
        if (cmdStart != std::string::npos) {
            job.command = remainder.substr(cmdStart);
        } else {
            return false;
        }

        job.rawLine = line;
        return true;
    }

    static std::vector<CronJob> load(const std::string& filepath) {
        std::vector<CronJob> jobs;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return jobs;
        }

        std::string line;
        while (std::getline(file, line)) {
            CronJob job;
            if (parseLine(line, job)) {
                jobs.push_back(job);
            }
        }
        return jobs;
    }

    static FILETIME getFileLastWriteTime(const std::string& filepath) {
        FILETIME ft = { 0 };
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fad)) {
            ft = fad.ftLastWriteTime;
        }
        return ft;
    }
};

static std::atomic<bool> g_cronRunning{true};

static BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[*] Shutting down Cron daemon..." << std::endl;
        g_cronRunning = false;
        return TRUE;
    }
    return FALSE;
}

