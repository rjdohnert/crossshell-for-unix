// ============================================================================
// 3. CALENDAR GENERATION ENGINE
// ============================================================================

class CalendarCalculator {
public:
    static bool isLeapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static int getDaysInMonth(int month, int year) {
        static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month == 2 && isLeapYear(year)) return 29;
        return (month >= 1 && month <= 12) ? days[month - 1] : 30;
    }

    static int getFirstDayOfMonth(int month, int year) {
        std::tm tmv{};
        tmv.tm_year = year - 1900;
        tmv.tm_mon = month - 1;
        tmv.tm_mday = 1;
        std::mktime(&tmv);
        return tmv.tm_wday; // 0 = Sunday
    }

    static std::string getMonthName(int month) {
        static const char* names[] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        return (month >= 1 && month <= 12) ? names[month - 1] : "";
    }

    static std::string renderMonth(int month, int year, bool mondayFirst) {
        std::ostringstream out;
        std::string title = getMonthName(month) + " " + std::to_string(year);
        int pad = (20 - static_cast<int>(title.size())) / 2;
        if (pad > 0) out << std::string(pad, ' ');
        out << title << "\n";

        if (mondayFirst) {
            out << "Mo Tu We Th Fr Sa Su\n";
        } else {
            out << "Su Mo Tu We Th Fr Sa\n";
        }

        int firstDay = getFirstDayOfMonth(month, year);
        if (mondayFirst) {
            firstDay = (firstDay + 6) % 7;
        }

        int daysInMonth = getDaysInMonth(month, year);

        for (int i = 0; i < firstDay; ++i) {
            out << "   ";
        }

        for (int d = 1; d <= daysInMonth; ++d) {
            out << std::setw(2) << d << " ";
            if ((firstDay + d) % 7 == 0) {
                out << "\n";
            }
        }
        out << "\n";
        return out.str();
    }
};

class CalEngine {
private:
    CalOptions options;

public:
    explicit CalEngine(CalOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::string content;
        if (options.fullYear || options.month == 0) {
            std::ostringstream ss;
            ss << "                             " << options.year << "\n\n";
            for (int m = 1; m <= 12; ++m) {
                ss << CalendarCalculator::renderMonth(m, options.year, options.mondayFirst) << "\n";
            }
            content = ss.str();
        } else {
            content = CalendarCalculator::renderMonth(options.month, options.year, options.mondayFirst);
        }

        return CalReporter::dispatch(content, options.outputFormat, options.pipeCommand);
    }
};

