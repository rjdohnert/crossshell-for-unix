/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 * See the repository license file for the full license text.
 */

#include "regex_engine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct RegexAtom {
    bool any;
    bool character_class;
    bool negated;
    bool repeated;
    unsigned char literal;
    unsigned char members[32];
} RegexAtom;

struct CompiledRegex {
    RegexAtom* atoms;
    size_t atom_count;
    bool anchored_start;
    bool anchored_end;
};

static bool RegexAtom_matches(const RegexAtom* atom, unsigned char value) {
    bool matched = atom->any || (!atom->character_class && atom->literal == value);
    if (atom->character_class) {
        matched = (atom->members[value / 8] & (unsigned char)(1u << (value % 8))) != 0;
        if (atom->negated) matched = !matched;
    }
    return matched;
}

static bool Regex_is_escaped(const char* pattern, size_t index) {
    size_t slash_count = 0;
    while (index > 0 && pattern[index - 1] == '\\') {
        slash_count++;
        index--;
    }
    return (slash_count % 2) != 0;
}

static bool Regex_compile_into(const char* pattern, CompiledRegex* regex) {
    size_t begin = 0;
    size_t end = strlen(pattern);

    if (begin < end && pattern[begin] == '^') {
        regex->anchored_start = true;
        begin++;
    }
    if (end > begin && pattern[end - 1] == '$' && !Regex_is_escaped(pattern, end - 1)) {
        regex->anchored_end = true;
        end--;
    }

    size_t capacity = end - begin;
    if (capacity > SIZE_MAX / sizeof(RegexAtom)) return false;
    if (capacity != 0) {
        regex->atoms = (RegexAtom*)calloc(capacity, sizeof(RegexAtom));
        if (!regex->atoms) return false;
    }

    while (begin < end) {
        RegexAtom* atom = &regex->atoms[regex->atom_count];
        unsigned char character = (unsigned char)pattern[begin++];
        if (character == '\\' && begin < end) {
            atom->literal = (unsigned char)pattern[begin++];
        } else if (character == '.') {
            atom->any = true;
        } else if (character == '[') {
            bool first = true;
            bool has_member = false;
            atom->character_class = true;
            if (begin < end && pattern[begin] == '^') {
                atom->negated = true;
                begin++;
            }
            while (begin < end && (pattern[begin] != ']' || first)) {
                unsigned char low;
                if (pattern[begin] == '\\' && begin + 1 < end) begin++;
                low = (unsigned char)pattern[begin++];
                first = false;

                if (begin + 1 < end && pattern[begin] == '-' && pattern[begin + 1] != ']') {
                    unsigned char high;
                    begin++;
                    if (pattern[begin] == '\\' && begin + 1 < end) begin++;
                    high = (unsigned char)pattern[begin++];
                    if (high < low) goto invalid_regex;
                    for (unsigned int value = low; value <= high; ++value) {
                        atom->members[value / 8] |= (unsigned char)(1u << (value % 8));
                    }
                } else {
                    atom->members[low / 8] |= (unsigned char)(1u << (low % 8));
                }
                has_member = true;
            }
            if (begin >= end || pattern[begin] != ']' || !has_member) goto invalid_regex;
            begin++;
        } else {
            atom->literal = character;
        }

        if (begin < end && pattern[begin] == '*') {
            atom->repeated = true;
            begin++;
        }
        regex->atom_count++;
    }
    return true;

invalid_regex:
    free(regex->atoms);
    regex->atoms = NULL;
    regex->atom_count = 0;
    return false;
}

CompiledRegex* RegexEngine_compile(const char* pattern) {
    if (!pattern) return NULL;
    CompiledRegex* regex = (CompiledRegex*)calloc(1, sizeof(CompiledRegex));
    if (!regex) return NULL;
    if (!Regex_compile_into(pattern, regex)) {
        RegexEngine_destroy(regex);
        return NULL;
    }
    return regex;
}

int RegexEngine_matches(const CompiledRegex* regex, const char* text) {
    if (!regex || !text) return 0;
    size_t text_length = strlen(text);
    if (regex->atom_count == 0) {
        return !(regex->anchored_start && regex->anchored_end) || text_length == 0;
    }
    if (text_length == SIZE_MAX || text_length + 1 > SIZE_MAX / sizeof(bool)) return -1;

    bool* previous = (bool*)calloc(text_length + 1, sizeof(bool));
    bool* current = (bool*)calloc(text_length + 1, sizeof(bool));
    if (!previous || !current) {
        free(previous);
        free(current);
        return -1;
    }
    if (regex->anchored_start) {
        previous[0] = true;
    } else {
        for (size_t j = 0; j <= text_length; ++j) previous[j] = true;
    }

    for (size_t i = 0; i < regex->atom_count; ++i) {
        const RegexAtom* atom = &regex->atoms[i];
        current[0] = atom->repeated && previous[0];
        for (size_t j = 1; j <= text_length; ++j) {
            bool matches = RegexAtom_matches(atom, (unsigned char)text[j - 1]);
            current[j] = atom->repeated
                ? previous[j] || (matches && current[j - 1])
                : matches && previous[j - 1];
        }

        if (i + 1 == regex->atom_count) {
            bool matched = false;
            if (regex->anchored_end) {
                matched = current[text_length];
            } else {
                for (size_t j = 0; j <= text_length; ++j) {
                    if (current[j]) {
                        matched = true;
                        break;
                    }
                }
            }
            if (matched) {
                free(previous);
                free(current);
                return 1;
            }
        }

        bool* swap = previous;
        previous = current;
        current = swap;
        memset(current, 0, (text_length + 1) * sizeof(bool));
    }
    free(previous);
    free(current);
    return 0;
}

void RegexEngine_destroy(CompiledRegex* regex) {
    if (!regex) return;
    free(regex->atoms);
    free(regex);
}
