#ifndef PV_INTERNAL_H
#define PV_INTERNAL_H

//licensed under the BSD 3-Clause License

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>

#define PV_VERSION "1.8.0"
#define DEFAULT_BUFFER_SIZE 65536
#define DEFAULT_INTERVAL 0.25

/* =========================================================================
 * Utilities: Parsing, Units, and Time
 * ========================================================================= */

/* =========================================================================
 * OOP: PvConfig
 * Encapsulates all flags matching UNIX pv qualifiers.
 * ========================================================================= */

typedef struct PvConfig {
    /* Modifiers */
    bool flag_progress;       /* -p, --progress */
    bool flag_timer;          /* -t, --timer */
    bool flag_eta;            /* -e, --eta */
    bool flag_fineta;         /* -I, --fineta */
    bool flag_rate;           /* -r, --rate */
    bool flag_average_rate;   /* -a, --average-rate */
    bool flag_bytes;          /* -b, --bytes */
    bool flag_bits;           /* -8, --bits */
    bool flag_bufpercent;     /* -T, --buffer-percent */
    int  flag_lastwritten;    /* -A, --last-written NUM */
    char format_string[256];  /* -F, --format FORMAT */
    bool flag_numeric;        /* -n, --numeric */
    bool flag_quiet;          /* -q, --quiet */

    /* Output modifiers */
    bool flag_wait;           /* -W, --wait */
    double delay_start;       /* -D, --delay-start SEC */
    uint64_t target_size;     /* -s, --size SIZE */
    bool flag_line_mode;      /* -l, --line-mode */
    bool flag_null;           /* -0, --null */
    double interval;          /* -i, --interval SEC */
    int term_width;           /* -w, --width WIDTH */
    int term_height;          /* -H, --height HEIGHT */
    char name[128];           /* -N, --name NAME */
    bool flag_force;          /* -f, --force */
    bool flag_cursor;         /* -c, --cursor */

    /* Data Transfer modifiers */
    uint64_t rate_limit;      /* -L, --rate-limit RATE */
    size_t buffer_size;       /* -B, --buffer-size SIZE */
    bool flag_skip_errors;    /* -E, --skip-errors */
    bool flag_stop_at_size;   /* -S, --stop-at-size */
    char pidfile[MAX_PATH];   /* -P, --pidfile FILE */

    /* Positional arguments: input files */
    char **input_files;
    int input_file_count;

    /* Class methods */
    void (*print_help)(void);
    void (*print_version)(void);
    bool (*parse_args)(struct PvConfig *self, int argc, char *argv[]);
} PvConfig;

/* =========================================================================
 * OOP: PvDisplay
 * Encapsulates terminal measurements, format strings, and console outputs.
 * ========================================================================= */

typedef struct PvDisplay {
    HANDLE h_stderr;
    bool is_tty;
    int width;
    double last_render_time;

    /* Class methods */
    int  (*get_terminal_width)(struct PvDisplay *self);
    void (*render)(struct PvDisplay *self, const PvConfig *cfg, uint64_t transferred,
                   uint64_t target_size, double elapsed, double current_rate, double avg_rate);
    void (*finish)(struct PvDisplay *self);
} PvDisplay;

/* =========================================================================
 * OOP: PvEngine
 * Encapsulates the I/O event loop, line processing, and rate throttling.
 * ========================================================================= */

typedef struct PvEngine {
    PvConfig  *config;
    PvDisplay *display;
    uint64_t  total_transferred;
    uint64_t  total_lines;
    double    start_time;
    double    last_update_time;
    uint64_t  transferred_since_last;

    /* Class methods */
    int  (*run)(struct PvEngine *self);
    bool (*process_file)(struct PvEngine *self, const char *filepath, char *buffer, size_t buf_sz);
} PvEngine;

/* =========================================================================
 * Application Entry Point
 * ========================================================================= */

/* Module interfaces */
int64_t get_qpc_frequency(void);
double get_time_sec(void);
uint64_t parse_size_suffix(const char *str);
void format_units(char *buf, size_t sz, uint64_t count, bool bits, bool lines);
void format_time(char *buf, size_t sz, double sec);
void PvConfig_print_version(void);
void PvConfig_print_help(void);
bool PvConfig_parse_args(PvConfig *self, int argc, char *argv[]);
PvConfig *PvConfig_New(void);
int PvDisplay_get_terminal_width(PvDisplay *self);
void PvDisplay_render(PvDisplay *self, const PvConfig *cfg, uint64_t transferred,
                             uint64_t target_size, double elapsed, double current_rate, double avg_rate);
void PvDisplay_finish(PvDisplay *self);
PvDisplay *PvDisplay_New(bool force);
bool PvEngine_process_file(PvEngine *self, const char *filepath, char *buffer, size_t buf_sz);
int PvEngine_run(PvEngine *self);
PvEngine *PvEngine_New(PvConfig *cfg, PvDisplay *disp);

#endif
