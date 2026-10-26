#include "pattern_set.h"
#include "allocation.h"
#include "literal_matcher.h"
#include "regex_matcher.h"
#include "search_state.h"

typedef struct {
    IMatcher base;
    IMatcher** children;
    size_t count;
} PatternSet;

static bool PatternSet_Find(IMatcher* base, const char* text, size_t length, size_t offset, MatchSpan* span) {
    PatternSet* set = (PatternSet*)base;
    rgx_search_work = 0;
    bool found = false; MatchSpan best = {0};
    for (size_t index = 0; index < set->count; ++index) {
        MatchSpan candidate;
        if (set->children[index]->vptr->FindMatch(set->children[index], text, length, offset, &candidate) && (!found || candidate.start < best.start)) { best = candidate; found = true; }
        if (rgx_match_error) return false;
        if(found && best.start==offset)break;
    }
    if (found && span) *span = best;
    return found;
}

static void PatternSet_Free(IMatcher* base) {
    PatternSet* set = (PatternSet*)base;
    for (size_t index = 0; index < set->count; ++index) set->children[index]->vptr->Destroy(set->children[index]);
    free(set->children); free(set);
}

static const IMatcherVTable PATTERN_SET_VTBL = {PatternSet_Find, PatternSet_Free};

IMatcher* PatternSet_New(const RgxOptions* options, bool ignore_case) {
    PatternSet* set = (PatternSet*)rgx_calloc(1, sizeof(PatternSet)); set->base.vptr = &PATTERN_SET_VTBL;
    set->children = (IMatcher**)rgx_calloc(options->pattern_count, sizeof(IMatcher*));
    size_t total_nodes = 0, total_states = 0, literal_bytes = 0;
    for (size_t index = 0; index < options->pattern_count; ++index) {
        IMatcher* child;
        if (options->fixed_strings) {
            size_t cost=strlen(options->patterns[index]);
            if(cost>1048576-literal_bytes){fprintf(stderr,"rgx: combined literals exceed 1 MiB\n");rgx_had_error=true;PatternSet_Free((IMatcher*)set);return NULL;}
            literal_bytes+=cost;
            child = LiteralMatcher_New(options->patterns[index], ignore_case, options->word_regexp);
            literal_matcher_set_full_line(child, options->line_regexp);
            literal_matcher_set_multiline(child,options->multiline);
        } else {
            char* wrapped = NULL;
            const char* pattern = options->patterns[index];
            if (options->line_regexp) { size_t length = strlen(pattern); wrapped = (char*)rgx_malloc(length + 8); snprintf(wrapped, length + 8, "^(?:%s)$", pattern); pattern = wrapped; }
            child = RegexMatcher_NewMode(pattern,ignore_case,options->word_regexp,options->multiline,options->multiline_dotall);
            free(wrapped);
        }
        if (!child) { PatternSet_Free((IMatcher*)set); return NULL; }
        if (!options->fixed_strings) {
            total_nodes += regex_matcher_node_count(child);
            total_states += regex_matcher_state_count(child);
            if (total_nodes > 32768 || total_states > 32768) {
                fprintf(stderr, "rgx: combined regex size exceeds safety limit\n");
                rgx_had_error = true;
                child->vptr->Destroy(child);
                PatternSet_Free((IMatcher*)set);
                return NULL;
            }
        }
        set->children[set->count++] = child;
    }
    return (IMatcher*)set;
}

/* ========================================================================= */
/*                          3. OOP PRINTER COMPONENT                         */
/* ========================================================================= */
