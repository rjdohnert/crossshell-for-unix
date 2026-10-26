#include "literal_matcher.h"
#include "allocation.h"
#include "unicode.h"
#include "search_state.h"

typedef struct LiteralMatcher {
    IMatcher base;
    char* needle;
    size_t needle_len;
    bool ignore_case;
    bool word_boundary;
    bool full_line;
    bool multiline;
    uint32_t* runes;
    size_t* failure;
    size_t* positions;
    size_t rune_count;
} LiteralMatcher;

static bool LiteralMatcher_FindMatch(IMatcher* super, const char* text, size_t text_len, size_t offset, MatchSpan* out_span) {
    LiteralMatcher* self = (LiteralMatcher*)super;
    if (offset > text_len) return false;
    if (self->full_line && !self->multiline && offset) return false;

    if (!self->rune_count) {
        for (size_t position = offset; position <= text_len;) {
            size_t width = 1;
            bool left = position && rune_word(previous_rune(text, position));
            bool right = position < text_len && rune_word(utf8_rune(text + position, text_len - position, &width));
            bool full_ok=!self->full_line || (!text_len) || (self->multiline &&
                (!position || text[position-1]=='\n') && (position==text_len || text[position]=='\n'));
            if (full_ok && (!self->word_boundary || (!left && !right))) {
                if (out_span) *out_span = (MatchSpan){position, 0};
                return true;
            }
            if (position == text_len || (self->full_line && !self->multiline)) break;
            position += width;
        }
        return false;
    }
    size_t matched = 0, rune_index = 0;
    for (size_t position = offset; position < text_len;) {
        if (++rgx_search_work > 50000000ULL) {
            fprintf(stderr, "rgx: literal search work limit exceeded\n");
            rgx_had_error = rgx_match_error = true;
            return false;
        }
        size_t width;
        uint32_t rune = utf8_rune(text + position, text_len - position, &width);
        if (self->ignore_case) rune = rune_fold(rune);
        self->positions[rune_index % self->rune_count] = position;
        while (matched && rune != self->runes[matched]) {
            if (++rgx_search_work > 50000000ULL) {
                fprintf(stderr, "rgx: literal search work limit exceeded\n");
                rgx_had_error = rgx_match_error = true;
                return false;
            }
            matched = self->failure[matched - 1];
        }
        if (rune == self->runes[matched]) ++matched;
        size_t end = position + width;
        if (matched == self->rune_count) {
            size_t begin = self->positions[(rune_index + 1 - self->rune_count) % self->rune_count];
            bool match = !self->full_line || (self->multiline?
                ((!begin || text[begin-1]=='\n') && (end==text_len || text[end]=='\n')):(!begin && end==text_len));
            if (self->word_boundary) {
                size_t boundary_width;
                bool start_ok = begin == 0 || !rune_word(previous_rune(text, begin));
                bool end_ok = end == text_len || !rune_word(utf8_rune(text + end, text_len - end, &boundary_width));
                match = match && start_ok && end_ok;
            }
            if (match && out_span) {
                out_span->start = begin;
                out_span->length = end - begin;
            }
            if (match) return true;
            matched = self->failure[matched - 1];
        }
        position = end;
        ++rune_index;
    }
    return false;
}

static void LiteralMatcher_Destroy(IMatcher* super) {
    LiteralMatcher* self = (LiteralMatcher*)super;
    if (self) {
        free(self->needle);
        free(self->runes);
        free(self->failure);
        free(self->positions);
        free(self);
    }
}

static const IMatcherVTable LITERAL_MATCHER_VTBL = {
    LiteralMatcher_FindMatch,
    LiteralMatcher_Destroy
};

IMatcher* LiteralMatcher_New(const char* pattern, bool ignore_case, bool word_boundary) {
    if(strlen(pattern)>65536){fprintf(stderr,"rgx: literal pattern exceeds 64 KiB\n");exit(2);}
    LiteralMatcher* m = (LiteralMatcher*)rgx_calloc(1, sizeof(LiteralMatcher));
    m->base.vptr = &LITERAL_MATCHER_VTBL;
    m->needle = rgx_strdup(pattern);
    m->needle_len = strlen(pattern);
    m->ignore_case = ignore_case;
    m->word_boundary = word_boundary;
    m->runes = (uint32_t*)rgx_calloc(m->needle_len + 1, sizeof(uint32_t));
    m->failure = (size_t*)rgx_calloc(m->needle_len + 1, sizeof(size_t));
    m->positions = (size_t*)rgx_calloc(m->needle_len + 1, sizeof(size_t));
    for (size_t position = 0; position < m->needle_len;) {
        size_t width;
        uint32_t rune = utf8_rune(pattern + position, m->needle_len - position, &width);
        m->runes[m->rune_count++] = ignore_case ? rune_fold(rune) : rune;
        position += width;
    }
    for (size_t index = 1, prefix = 0; index < m->rune_count; ++index) {
        while (prefix && m->runes[index] != m->runes[prefix]) prefix = m->failure[prefix - 1];
        if (m->runes[index] == m->runes[prefix]) ++prefix;
        m->failure[index] = prefix;
    }
    return (IMatcher*)m;
}

/* --- Bounded, ordered Thompson NFA; all regex implementation stays here. --- */
void literal_matcher_set_full_line(IMatcher* matcher, bool full_line) {
    ((LiteralMatcher*)matcher)->full_line = full_line;
}

void literal_matcher_set_multiline(IMatcher* matcher,bool multiline) {
    ((LiteralMatcher*)matcher)->multiline=multiline;
}
