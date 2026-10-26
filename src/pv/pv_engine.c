#include "pv_internal.h"

bool PvEngine_process_file(PvEngine *self, const char *filepath, char *buffer, size_t buf_sz) {
    FILE *in = stdin;
    bool is_pipe = false;

    if (filepath != NULL) {
        if (fopen_s(&in, filepath, "rb") != 0 || !in) {
            fprintf(stderr, "\npv: cannot open '%s': %s\n", filepath, strerror(errno));
            return false;
        }
        /* If size not explicitly specified, resolve file size */
        if (self->config->target_size == 0) {
            _fseeki64(in, 0, SEEK_END);
            self->config->target_size = _ftelli64(in);
            _fseeki64(in, 0, SEEK_SET);
        }
    } else {
        is_pipe = true;
    }

    bool first_chunk = true;
    double period_start = get_time_sec();
    uint64_t period_bytes = 0;

    while (1) {
        /* Check termination size condition */
        if (self->config->flag_stop_at_size && self->config->target_size > 0) {
            if (self->total_transferred >= self->config->target_size) break;
        }

        size_t to_read = buf_sz;
        if (self->config->flag_stop_at_size && self->config->target_size > 0) {
            uint64_t remain = self->config->target_size - self->total_transferred;
            if (remain < (uint64_t)to_read) to_read = (size_t)remain;
        }

        size_t n_read = fread(buffer, 1, to_read, in);
        if (n_read == 0) {
            if (ferror(in)) {
                if (self->config->flag_skip_errors) {
                    clearerr(in);
                    continue;
                }
            }
            break; /* EOF */
        }

        /* Write data directly to stdout */
        size_t n_written = fwrite(buffer, 1, n_read, stdout);
        if (n_written < n_read) {
            fprintf(stderr, "\npv: write error: %s\n", strerror(errno));
            if (!is_pipe) fclose(in);
            return false;
        }

        /* Update counters */
        self->total_transferred += n_written;
        self->transferred_since_last += n_written;
        period_bytes += n_written;

        /* Line counting */
        if (self->config->flag_line_mode) {
            char target_char = self->config->flag_null ? '\0' : '\n';
            for (size_t i = 0; i < n_written; i++) {
                if (buffer[i] == target_char) self->total_lines++;
            }
        }

        double now = get_time_sec();

        /* Rate limiting */
        if (self->config->rate_limit > 0) {
            double expected_duration = (double)period_bytes / (double)self->config->rate_limit;
            double actual_duration = now - period_start;
            if (actual_duration < expected_duration) {
                DWORD sleep_ms = (DWORD)((expected_duration - actual_duration) * 1000.0);
                if (sleep_ms > 0) {
                    Sleep(sleep_ms);
                    now = get_time_sec();
                }
            }
            if (actual_duration >= 1.0) {
                period_bytes = 0;
                period_start = now;
            }
        }

        if (first_chunk) {
            if (self->start_time == 0.0) self->start_time = now;
            first_chunk = false;
        }

        /* UI Rendering trigger */
        if ((now - self->last_update_time) >= self->config->interval) {
            if (!(self->config->flag_wait && self->total_transferred == 0)) {
                double elapsed = now - self->start_time;
                if (elapsed >= self->config->delay_start) {
                    double dt = now - self->last_update_time;
                    double cur_rate = (dt > 0.0) ? ((double)self->transferred_since_last / dt) : 0.0;
                    double avg_rate = (elapsed > 0.0) ? ((double)self->total_transferred / elapsed) : 0.0;
                    uint64_t count = self->config->flag_line_mode ? self->total_lines : self->total_transferred;

                    self->display->render(self->display, self->config, count,
                                          self->config->target_size, elapsed, cur_rate, avg_rate);
                }
            }
            self->transferred_since_last = 0;
            self->last_update_time = now;
        }
    }

    if (!is_pipe) fclose(in);
    return true;
}

int PvEngine_run(PvEngine *self) {
    /* Write PID file if requested */
    if (self->config->pidfile[0] != '\0') {
        FILE *pf;
        if (fopen_s(&pf, self->config->pidfile, "w") == 0 && pf) {
            fprintf(pf, "%lu\n", GetCurrentProcessId());
            fclose(pf);
        }
    }

    char *buffer = (char *)malloc(self->config->buffer_size);
    if (!buffer) {
        fprintf(stderr, "pv: out of memory allocating buffer\n");
        return 1;
    }

    self->start_time = get_time_sec();
    self->last_update_time = self->start_time;

    if (self->config->input_file_count == 0) {
        /* Read from Stdin */
        self->process_file(self, NULL, buffer, self->config->buffer_size);
    } else {
        /* Iterate through command line files */
        for (int i = 0; i < self->config->input_file_count; i++) {
            if (strcmp(self->config->input_files[i], "-") == 0) {
                self->process_file(self, NULL, buffer, self->config->buffer_size);
            } else {
                self->process_file(self, self->config->input_files[i], buffer, self->config->buffer_size);
            }
        }
    }

    /* Final statistics render */
    double now = get_time_sec();
    double elapsed = now - self->start_time;
    double avg_rate = (elapsed > 0.0) ? ((double)self->total_transferred / elapsed) : 0.0;
    uint64_t count = self->config->flag_line_mode ? self->total_lines : self->total_transferred;

    if (!self->config->flag_quiet && !self->config->flag_numeric) {
        self->display->render(self->display, self->config, count,
                              self->config->target_size, elapsed, avg_rate, avg_rate);
        self->display->finish(self->display);
    }

    free(buffer);
    if (self->config->pidfile[0] != '\0') {
        DeleteFileA(self->config->pidfile);
    }
    return 0;
}

PvEngine *PvEngine_New(PvConfig *cfg, PvDisplay *disp) {
    PvEngine *self = (PvEngine *)calloc(1, sizeof(PvEngine));
    self->config = cfg;
    self->display = disp;
    self->process_file = PvEngine_process_file;
    self->run = PvEngine_run;
    return self;
}
