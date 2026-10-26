#include "jq_internal.h"

char* JqEngine_read_stream_impl(FILE *stream) {
    size_t cap = 16384;
    size_t len = 0;
    char *buf = (char *)malloc(cap);

    while (!feof(stream) && !ferror(stream)) {
        size_t n = fread(buf + len, 1, cap - len - 1, stream);
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        if (n == 0) break;
    }
    buf[len] = '\0';
    return buf;
}

void JqEngine_process_value_impl(JqEngine *self, JqValue *val) {
    JqResultSet results = self->filter->vptr->execute(self->filter, val);

    for (size_t i = 0; i < results.count; i++) {
        JqValue *item = results.items[i];
        if (item->vptr->is_truthy(item)) {
            self->has_truthy_output = true;
        }

        if (self->config->raw_output && item->type == JQ_STRING) {
            fputs(item->as.str_val, stdout);
            if (!self->config->join_output) fputc('\n', stdout);
        } else {
            item->vptr->serialize(item, self->serializer, 0);
            fputc('\n', stdout);
        }
        if (self->config->unbuffered) {
            fflush(stdout);
        }
    }

    JqResultSet_Free(&results);
}

int JqEngine_run_impl(JqEngine *self) {
    /* If --null-input / -n */
    if (self->config->null_input) {
        JqValue *null_v = JqValue_NewNull();
        self->process_value(self, null_v);
        null_v->vptr->destroy(null_v);
        return (self->config->exit_status && !self->has_truthy_output) ? 1 : 0;
    }

    /* Iterate Input Streams */
    int file_idx = 0;
    do {
        FILE *in = stdin;
        bool is_stdin = true;

        if (self->config->input_file_count > 0) {
            const char *path = self->config->input_files[file_idx];
            if (strcmp(path, "-") != 0) {
                if (fopen_s(&in, path, "rb") != 0 || !in) {
                    fprintf(stderr, "jq: cannot open %s\n", path);
                    return 2;
                }
                is_stdin = false;
            }
        }

        if (self->config->raw_input) {
            /* Raw Input line-by-line */
            char line[8192];
            JqValue *slurp_arr = self->config->slurp ? JqValue_NewArray() : NULL;

            while (fgets(line, sizeof(line), in)) {
                size_t l = strlen(line);
                while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
                JqValue *str_val = JqValue_NewString(line);

                if (self->config->slurp) {
                    JqValue_ArrayAppend(slurp_arr, str_val);
                } else {
                    self->process_value(self, str_val);
                    str_val->vptr->destroy(str_val);
                }
            }
            if (slurp_arr) {
                self->process_value(self, slurp_arr);
                slurp_arr->vptr->destroy(slurp_arr);
            }
        } else {
            /* Standard JSON parsing */
            char *json_content = self->read_stream(in);
            JqParser *parser = JqParser_New(json_content);

            if (self->config->slurp) {
                JqValue *slurp_arr = JqValue_NewArray();
                JqValue *val;
                while ((val = parser->parse_value(parser)) != NULL) {
                    JqValue_ArrayAppend(slurp_arr, val);
                }
                self->process_value(self, slurp_arr);
                slurp_arr->vptr->destroy(slurp_arr);
            } else {
                JqValue *val;
                while ((val = parser->parse_value(parser)) != NULL) {
                    self->process_value(self, val);
                    val->vptr->destroy(val);
                }
            }

            free(parser);
            free(json_content);
        }

        if (!is_stdin) fclose(in);
        file_idx++;
    } while (file_idx < self->config->input_file_count);

    if (self->config->exit_status && !self->has_truthy_output) {
        return 1;
    }
    return 0;
}

JqEngine *JqEngine_New(JqConfig *cfg) {
    JqEngine *eng = (JqEngine *)calloc(1, sizeof(JqEngine));
    eng->config = cfg;
    eng->serializer = JqSerializer_New(cfg, stdout);
    eng->filter = JqFilter_Compile(cfg->filter_string);
    eng->run = JqEngine_run_impl;
    eng->process_value = JqEngine_process_value_impl;
    eng->read_stream = JqEngine_read_stream_impl;
    return eng;
}

void JqEngine_Destroy(JqEngine *self) {
    if (!self) return;
    if (self->filter) self->filter->vptr->destroy(self->filter);
    free(self->serializer);
    free(self->config->filter_string);
    for (int i = 0; i < self->config->named_arg_count; i++) {
        self->config->named_args[i].val->vptr->destroy(self->config->named_args[i].val);
    }
    free(self->config);
    free(self);
}
