#include "pv_internal.h"

int PvDisplay_get_terminal_width(PvDisplay *self) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(self->h_stderr, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

void PvDisplay_render(PvDisplay *self, const PvConfig *cfg, uint64_t transferred,
                             uint64_t target_size, double elapsed, double current_rate, double avg_rate) {
    if (cfg->flag_quiet) return;

    if (cfg->flag_numeric) {
        if (target_size > 0) {
            int pct = (int)((transferred * 100ULL) / target_size);
            if (pct > 100) pct = 100;
            fprintf(stderr, "%d\n", pct);
        } else {
            fprintf(stderr, "%llu\n", (unsigned long long)transferred);
        }
        return;
    }

    int term_w = (cfg->term_width > 0) ? cfg->term_width : self->get_terminal_width(self);
    if (term_w < 20) term_w = 20;

    char line_buf[1024];
    char prefix[128] = "";
    char info[512] = "";

    if (cfg->name[0] != '\0') {
        snprintf(prefix, sizeof(prefix), "%s: ", cfg->name);
    }

    /* 1. Bytes/Count */
    if (cfg->flag_bytes) {
        char s_b[64];
        format_units(s_b, sizeof(s_b), transferred, cfg->flag_bits, cfg->flag_line_mode);
        snprintf(info + strlen(info), sizeof(info) - strlen(info), "%s ", s_b);
    }

    /* 2. Timer */
    if (cfg->flag_timer) {
        char s_t[64];
        format_time(s_t, sizeof(s_t), elapsed);
        snprintf(info + strlen(info), sizeof(info) - strlen(info), "%s ", s_t);
    }

    /* 3. Rate */
    if (cfg->flag_rate) {
        char s_r[64];
        format_units(s_r, sizeof(s_r), (uint64_t)current_rate, cfg->flag_bits, false);
        snprintf(info + strlen(info), sizeof(info) - strlen(info), "[%s/s] ", s_r);
    }

    /* 4. Average Rate */
    if (cfg->flag_average_rate) {
        char s_ar[64];
        format_units(s_ar, sizeof(s_ar), (uint64_t)avg_rate, cfg->flag_bits, false);
        snprintf(info + strlen(info), sizeof(info) - strlen(info), "[avg: %s/s] ", s_ar);
    }

    /* 5. ETA and Finish Time */
    char eta_buf[64] = "";
    if ((cfg->flag_eta || cfg->flag_fineta) && target_size > 0 && transferred > 0) {
        double rem_bytes = (target_size > transferred) ? (double)(target_size - transferred) : 0.0;
        double speed = (avg_rate > 0) ? avg_rate : current_rate;
        double eta_sec = (speed > 0) ? (rem_bytes / speed) : 0.0;

        if (cfg->flag_eta) {
            char t_eta[32];
            format_time(t_eta, sizeof(t_eta), eta_sec);
            snprintf(eta_buf + strlen(eta_buf), sizeof(eta_buf) - strlen(eta_buf), "ETA %s ", t_eta);
        }
        if (cfg->flag_fineta) {
            time_t fin_t = time(NULL) + (time_t)eta_sec;
            struct tm lt;
            char t_fin[32];
            if (localtime_s(&lt, &fin_t) == 0 &&
                strftime(t_fin, sizeof(t_fin), "%H:%M:%S", &lt) > 0) {
                snprintf(eta_buf + strlen(eta_buf), sizeof(eta_buf) - strlen(eta_buf), "Fin %s ", t_fin);
            }
        }
    }

    /* 6. Assemble line & Progress Bar */
    if (cfg->flag_progress && target_size > 0) {
        int pct = (int)((transferred * 100ULL) / target_size);
        if (pct > 100) pct = 100;

        char bar_fixed[256];
        snprintf(bar_fixed, sizeof(bar_fixed), "%s%s%s%3d%% [", prefix, info, eta_buf, pct);
        int bar_start_len = (int)strlen(bar_fixed);
        int avail_bar_len = term_w - bar_start_len - 2; /* space for ']' and '\r' */

        if (avail_bar_len > 3) {
            int fill = (avail_bar_len * pct) / 100;
            char bar[256];
            int bidx = 0;
            for (int i = 0; i < fill; i++) bar[bidx++] = '=';
            if (fill < avail_bar_len) {
                bar[bidx++] = '>';
                for (int i = fill + 1; i < avail_bar_len; i++) bar[bidx++] = ' ';
            }
            bar[bidx] = '\0';
            snprintf(line_buf, sizeof(line_buf), "%s%s]", bar_fixed, bar);
        } else {
            snprintf(line_buf, sizeof(line_buf), "%s%s%3d%%", prefix, info, pct);
        }
    } else {
        snprintf(line_buf, sizeof(line_buf), "%s%s%s", prefix, info, eta_buf);
    }

    /* Truncate or Pad to clear remnants of longer previous line */
    int cur_len = (int)strlen(line_buf);
    if (cur_len > term_w - 1) {
        line_buf[term_w - 1] = '\0';
    } else {
        while (cur_len < term_w - 1 && cur_len < (int)sizeof(line_buf) - 2) {
            line_buf[cur_len++] = ' ';
        }
        line_buf[cur_len] = '\0';
    }

    fprintf(stderr, "\r%s", line_buf);
    fflush(stderr);
}

void PvDisplay_finish(PvDisplay *self) {
    (void)self;
    fprintf(stderr, "\n");
    fflush(stderr);
}

PvDisplay *PvDisplay_New(bool force) {
    PvDisplay *self = (PvDisplay *)calloc(1, sizeof(PvDisplay));
    self->h_stderr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode;
    self->is_tty = GetConsoleMode(self->h_stderr, &mode) || force;
    self->get_terminal_width = PvDisplay_get_terminal_width;
    self->render = PvDisplay_render;
    self->finish = PvDisplay_finish;
    return self;
}
