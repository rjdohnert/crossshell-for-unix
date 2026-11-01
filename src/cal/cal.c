#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CELL_WIDTH 20
// 8 lines per month grid: 1 title + 1 day-label row + 6 week rows
// (6 weeks is the maximum any month can span).
#define MONTH_LINES 8

#define CAL_VERSION "1.1.0"

static const char *MONTH_NAMES[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

typedef struct {
    int lines_count;
    char line[MONTH_LINES][64]; // extra space for ANSI escape codes
} MonthGrid;

// Enable ANSI escape sequences on Windows ConHost / Windows Terminal
static void enable_virtual_terminal(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
}

static int is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && is_leap_year(y)) return 29;
    return dim[m - 1];
}

// Sakamoto's algorithm: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
static int day_of_week(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Generates an 8-line text grid for a single month (width = 20 chars visually)
static void render_month(int year, int month, int monday_first, int show_year_in_title,
                         int today_y, int today_m, int today_d, MonthGrid *grid) {
    grid->lines_count = MONTH_LINES;

    // Line 0: Header (Month Name or Month Name + Year)
    char header_raw[32];
    if (show_year_in_title) {
        snprintf(header_raw, sizeof(header_raw), "%s %d", MONTH_NAMES[month - 1], year);
    } else {
        snprintf(header_raw, sizeof(header_raw), "%s", MONTH_NAMES[month - 1]);
    }

    int len = (int)strlen(header_raw);
    int pad_left = (CELL_WIDTH - len) / 2;
    if (pad_left < 0) pad_left = 0;
    int pad_right = CELL_WIDTH - pad_left - len;
    if (pad_right < 0) pad_right = 0;

    snprintf(grid->line[0], sizeof(grid->line[0]), "%*s%s%*s", pad_left, "", header_raw, pad_right, "");

    // Line 1: Day of week labels
    if (monday_first) {
        snprintf(grid->line[1], sizeof(grid->line[1]), "Mo Tu We Th Fr Sa Su");
    } else {
        snprintf(grid->line[1], sizeof(grid->line[1]), "Su Mo Tu We Th Fr Sa");
    }

    // Lines 2 to 7: Days grid
    int first_dow = day_of_week(year, month, 1);
    int start_col = monday_first ? (first_dow + 6) % 7 : first_dow;
    int total_days = days_in_month(year, month);

    int cur_day = 1;
    for (int r = 2; r < MONTH_LINES; r++) {
        char *ptr = grid->line[r];
        size_t rem = sizeof(grid->line[r]);
        ptr[0] = '\0';

        for (int c = 0; c < 7; c++) {
            if (r == 2 && c < start_col) {
                // Leading spaces before day 1
                int written = snprintf(ptr, rem, (c == 6) ? "  " : "   ");
                ptr += written; rem -= written;
            } else if (cur_day <= total_days) {
                int is_today = (year == today_y && month == today_m && cur_day == today_d);
                int written;
                if (is_today) {
                    written = snprintf(ptr, rem, (c == 6) ? "\x1b[7m%2d\x1b[0m" : "\x1b[7m%2d\x1b[0m ", cur_day);
                } else {
                    written = snprintf(ptr, rem, (c == 6) ? "%2d" : "%2d ", cur_day);
                }
                ptr += written; rem -= written;
                cur_day++;
            } else {
                // Trailing spaces after month ends
                int written = snprintf(ptr, rem, (c == 6) ? "  " : "   ");
                ptr += written; rem -= written;
            }
        }
    }

    // Trim trailing all-space week rows (single-month output stays compact)
    while (grid->lines_count > 2) {
        const char *row = grid->line[grid->lines_count - 1];
        int blank = 1;
        for (int i = 0; row[i]; i++) {
            if (row[i] != ' ') { blank = 0; break; }
        }
        if (!blank) break;
        grid->lines_count--;
    }
}

// Print 1 to 3 months side by side
static void print_side_by_side(int count, MonthGrid grids[3]) {
    for (int row = 0; row < MONTH_LINES; row++) {
        for (int i = 0; i < count; i++) {
            // Grids with trimmed trailing rows pad with spaces to keep columns aligned
            if (row < grids[i].lines_count) {
                printf("%s", grids[i].line[row]);
            } else {
                printf("%*s", CELL_WIDTH, "");
            }
            if (i < count - 1) printf("  ");
        }
        printf("\n");
    }
}

// Print full year
static void print_year(int year, int monday_first, int today_y, int today_m, int today_d) {
    printf("%34d\n\n", year);

    MonthGrid grids[3];
    for (int quarter = 0; quarter < 4; quarter++) {
        for (int m = 0; m < 3; m++) {
            int month_num = quarter * 3 + m + 1;
            render_month(year, month_num, monday_first, 0, today_y, today_m, today_d, &grids[m]);
        }
        print_side_by_side(3, grids);
        if (quarter < 3) printf("\n");
    }
}

// Adjust month / year backwards or forwards
static void offset_month(int *year, int *month, int delta) {
    *month += delta;
    while (*month < 1) {
        *month += 12;
        *year -= 1;
    }
    while (*month > 12) {
        *month -= 12;
        *year += 1;
    }
}

static void print_usage(void) {
    printf("Usage: cal [options] [month] [year]\n"
           "       cal [options] <year>\n\n"
           "Options:\n"
           "  -1              Display single month output (default)\n"
           "  -3              Display previous, current and next month\n"
           "  -y              Display a calendar for the current year\n"
           "  -m              Display Monday as the first day of the week\n"
           "  -s              Display Sunday as the first day of the week (default)\n"
           "  -h, --help      Display this help message\n"
           "  -V, --version   Display version information\n");
}

// Strictly parse a decimal integer; rejects empty strings, trailing
// garbage, and out-of-int-range values. Returns 1 on success.
static int parse_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

int main(int argc, char *argv[]) {
    enable_virtual_terminal();

    // Get current local system date
    SYSTEMTIME st;
    GetLocalTime(&st);
    int today_y = st.wYear;
    int today_m = st.wMonth;
    int today_d = st.wDay;

    int monday_first = 0;
    int mode = 1; // 1 = single month, 3 = three months, 12 = full year
    int positional[2];
    int pos_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-1") == 0) {
                mode = 1;
            } else if (strcmp(argv[i], "-3") == 0) {
                mode = 3;
            } else if (strcmp(argv[i], "-y") == 0) {
                mode = 12;
            } else if (strcmp(argv[i], "-m") == 0) {
                monday_first = 1;
            } else if (strcmp(argv[i], "-s") == 0) {
                monday_first = 0;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
                printf("cal version %s\n", CAL_VERSION);
                return 0;
            } else {
                fprintf(stderr, "cal: unknown option -- %s\n", argv[i]);
                print_usage();
                return 1;
            }
        } else {
            if (pos_count >= 2) {
                fprintf(stderr, "cal: too many arguments -- %s\n", argv[i]);
                print_usage();
                return 1;
            }
            if (!parse_int(argv[i], &positional[pos_count])) {
                fprintf(stderr, "cal: invalid number -- %s\n", argv[i]);
                return 1;
            }
            pos_count++;
        }
    }

    int target_month = today_m;
    int target_year = today_y;

    if (pos_count == 1) {
        // e.g. `cal 2026` -> full year
        target_year = positional[0];
        mode = 12;
    } else if (pos_count == 2) {
        // e.g. `cal 10 2026`
        target_month = positional[0];
        target_year = positional[1];
        if (target_month < 1 || target_month > 12) {
            fprintf(stderr, "cal: %d is not a valid month (1-12)\n", target_month);
            return 1;
        }
    }

    if (target_year < 1 || target_year > 9999) {
        fprintf(stderr, "cal: year %d is out of range (1-9999)\n", target_year);
        return 1;
    }

    if (mode == 1) {
        MonthGrid g;
        render_month(target_year, target_month, monday_first, 1, today_y, today_m, today_d, &g);
        for (int r = 0; r < g.lines_count; r++) {
            printf("%s\n", g.line[r]);
        }
    } else if (mode == 3) {
        MonthGrid grids[3];
        int y[3] = {target_year, target_year, target_year};
        int m[3] = {target_month, target_month, target_month};
        offset_month(&y[0], &m[0], -1);
        offset_month(&y[2], &m[2], 1);

        for (int i = 0; i < 3; i++) {
            render_month(y[i], m[i], monday_first, 1, today_y, today_m, today_d, &grids[i]);
        }
        print_side_by_side(3, grids);
    } else if (mode == 12) {
        print_year(target_year, monday_first, today_y, today_m, today_d);
    }

    return 0;
}