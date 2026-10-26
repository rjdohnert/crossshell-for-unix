#include "regex_matcher.h"
#include "allocation.h"
#include "unicode.h"
#include "search_state.h"

#define RX_LIMIT 8192
#define RX_DEPTH 128
#define RX_TERMS 32
#define RX_WORK_LIMIT 50000000ULL

typedef enum { RX_EMPTY, RX_CHAR, RX_ANY, RX_CLASS, RX_BOL, RX_EOL, RX_ABS_BOL, RX_ABS_EOL, RX_WORD, RX_NOTWORD, RX_LEFTWORD, RX_RIGHTWORD, RX_CONCAT, RX_ALT, RX_REPEAT, RX_SPLIT, RX_MATCH } RxOp;
typedef struct { uint32_t low, high; int category; bool inverse; } RxTerm;
typedef struct {
    RxOp op; int left, right, minimum, maximum; uint32_t rune;
    bool ignore_case, lazy, inverted; int term_count; RxTerm terms[RX_TERMS];
    bool multiline, dotall;
} RxNode;
typedef struct { RxOp op; int next, alternate, node; } RxInstruction;
typedef struct { int instruction; size_t start; } RxThread;
typedef struct RegexMatcher {
    IMatcher base; RxNode* nodes; size_t node_count, node_capacity;
    RxInstruction* code; size_t code_count, code_capacity; int start;
    const char* cursor; const char* error; bool ignore_case; unsigned depth;
    bool multiline, dotall;
    RxThread *current, *next, *stack; unsigned char* seen; uint64_t work;
} RegexMatcher;

static int rx_node(RegexMatcher* matcher, RxNode node) {
    if (matcher->node_count == RX_LIMIT) { matcher->error = "regex exceeds node limit"; return -1; }
    if (matcher->node_count == matcher->node_capacity) {
        matcher->node_capacity = matcher->node_capacity ? matcher->node_capacity * 2 : 32;
        matcher->nodes = (RxNode*)rgx_realloc_array(matcher->nodes, matcher->node_capacity, sizeof(RxNode));
    }
    matcher->nodes[matcher->node_count] = node;
    return (int)matcher->node_count++;
}

static RxTerm rx_escape(RegexMatcher* matcher, bool in_class, RxOp* assertion) {
    RxTerm term = {0}; char escape = *matcher->cursor++;
    if (!escape) { --matcher->cursor; matcher->error = "trailing backslash"; return term; }
    if (escape == 'p' || escape == 'P') {
        char name[64]; size_t length = 0;
        bool braces = *matcher->cursor == '{';
        if (braces) ++matcher->cursor;
        while (*matcher->cursor && (braces ? *matcher->cursor != '}' : length == 0)) {
            if (length + 1 == sizeof(name)) { matcher->error = "Unicode property name too long"; return term; }
            name[length++] = *matcher->cursor++;
        }
        name[length] = 0;
        if (!length || (braces && *matcher->cursor != '}')) { matcher->error = "invalid Unicode property"; return term; }
        if (braces) ++matcher->cursor;
        term.category = unicode_property_id(name); term.inverse = escape == 'P';
        if (!term.category) matcher->error = "unsupported Unicode property";
        return term;
    }
    if (escape == 'd' || escape == 'D' || escape == 'w' || escape == 'W' || escape == 's' || escape == 'S') {
        term.category = escape == 'd' || escape == 'D' ? 1 : escape == 'w' || escape == 'W' ? 2 : 3;
        term.inverse = escape == 'D' || escape == 'W' || escape == 'S'; return term;
    }
    if (!in_class && (escape == 'b' || escape == 'B' || escape == 'A' || escape == 'z')) {
        *assertion = escape == 'b' ? RX_WORD : escape == 'B' ? RX_NOTWORD : escape == 'A' ? RX_ABS_BOL : RX_ABS_EOL; return term;
    }
    if (escape == 'n') term.low = '\n'; else if (escape == 'r') term.low = '\r';
    else if (escape == 't') term.low = '\t'; else if (escape == 'f') term.low = '\f';
    else if (escape == 'v') term.low = '\v'; else if (escape == 'a') term.low = '\a';
    else if (escape == 'b' && in_class) term.low = '\b';
    else if (escape == 'x' || escape == 'u') {
        unsigned digits = escape == 'x' ? 2 : 4; bool braces = *matcher->cursor == '{';
        if (braces) { ++matcher->cursor; digits = 6; }
        unsigned consumed = 0;
        while (consumed < digits && isxdigit((unsigned char)*matcher->cursor)) {
            char digit = *matcher->cursor++; term.low = (term.low << 4) | (uint32_t)(isdigit((unsigned char)digit) ? digit - '0' : tolower((unsigned char)digit) - 'a' + 10); ++consumed;
        }
        if ((!braces && consumed != digits) || (braces && (!consumed || *matcher->cursor != '}')) || term.low > 0x10FFFF || (term.low >= 0xD800 && term.low <= 0xDFFF)) matcher->error = "invalid hexadecimal escape";
        else if (braces) ++matcher->cursor;
    } else if (isalnum((unsigned char)escape)) matcher->error = "unsupported escape (backreferences and Unicode properties are not supported)";
    else term.low = (unsigned char)escape;
    term.high = term.low; return term;
}

static int rx_expression(RegexMatcher* matcher);
static int rx_atom(RegexMatcher* matcher) {
    RxNode node = {0}; node.ignore_case = matcher->ignore_case;
    node.multiline=matcher->multiline;node.dotall=matcher->dotall;
    char token = *matcher->cursor++;
    if (token == '(') {
        if (++matcher->depth > RX_DEPTH) { matcher->error = "regex nesting limit exceeded"; return -1; }
        bool old_case = matcher->ignore_case;
        bool old_multiline=matcher->multiline, old_dotall=matcher->dotall;
        if (*matcher->cursor == '?') {
            ++matcher->cursor;
            if (*matcher->cursor != ':') {
                bool enable = true, flag = false;
                while (*matcher->cursor && *matcher->cursor != ':' && *matcher->cursor != ')') {
                    char option = *matcher->cursor++;
                    if (option == '-') enable = false;
                    else if (option == 'i') { matcher->ignore_case = enable; flag = true; }
                    else if(option=='m'){matcher->multiline=enable;flag=true;}
                    else if(option=='s'){matcher->dotall=enable;flag=true;}
                    else { matcher->error = "unsupported group or inline flag"; return -1; }
                }
                if (!flag) { matcher->error = "unsupported group"; return -1; }
                if (*matcher->cursor == ')') { ++matcher->cursor; --matcher->depth; return rx_node(matcher, node); }
            }
            if (*matcher->cursor != ':') { matcher->error = "unterminated flag group"; return -1; }
            ++matcher->cursor;
        }
        int child = rx_expression(matcher);
        if (*matcher->cursor != ')') { matcher->error = "unclosed group"; return -1; }
        ++matcher->cursor; --matcher->depth; matcher->ignore_case = old_case;matcher->multiline=old_multiline;matcher->dotall=old_dotall; return child;
    }
    if (token == '[') {
        node.op = RX_CLASS;
        if (*matcher->cursor == '^') { node.inverted = true; ++matcher->cursor; }
        while (*matcher->cursor && (*matcher->cursor != ']' || node.term_count == 0)) {
            if (node.term_count == RX_TERMS) { matcher->error = "character class exceeds range limit"; return -1; }
            RxTerm term = {0}; RxOp assertion = RX_EMPTY;
            if (*matcher->cursor == '\\') { ++matcher->cursor; term = rx_escape(matcher, true, &assertion); }
            else { size_t width; term.low = utf8_rune(matcher->cursor, strlen(matcher->cursor), &width); matcher->cursor += width; term.high = term.low; }
            if (!term.category && *matcher->cursor == '-' && matcher->cursor[1] && matcher->cursor[1] != ']') {
                ++matcher->cursor; RxTerm end = {0};
                if (*matcher->cursor == '\\') { ++matcher->cursor; end = rx_escape(matcher, true, &assertion); }
                else { size_t width; end.low = utf8_rune(matcher->cursor, strlen(matcher->cursor), &width); matcher->cursor += width; }
                if (end.category || end.low < term.low) { matcher->error = "invalid character range"; return -1; }
                term.high = end.low;
            }
            node.terms[node.term_count++] = term;
            if (matcher->error) return -1;
        }
        if (*matcher->cursor != ']') { matcher->error = "unclosed character class"; return -1; }
        ++matcher->cursor;
    } else if (token == '.') node.op = RX_ANY;
    else if (token == '^') node.op = RX_BOL;
    else if (token == '$') node.op = RX_EOL;
    else if (token == '\\') {
        RxOp assertion = RX_EMPTY; RxTerm term = rx_escape(matcher, false, &assertion);
        if (assertion != RX_EMPTY) node.op = assertion;
        else if (term.category) { node.op = RX_CLASS; node.term_count = 1; node.terms[0] = term; }
        else { node.op = RX_CHAR; node.rune = term.low; }
    } else if (token == '*' || token == '+' || token == '?' || token == '{' || token == '}') { matcher->error = "repetition without a valid operand"; return -1; }
    else { size_t width; --matcher->cursor; node.op = RX_CHAR; node.rune = utf8_rune(matcher->cursor, strlen(matcher->cursor), &width); matcher->cursor += width; }
    return matcher->error ? -1 : rx_node(matcher, node);
}

static int rx_number(RegexMatcher* matcher) {
    int value = 0;
    if (!isdigit((unsigned char)*matcher->cursor)) { matcher->error = "expected repetition count"; return 0; }
    while (isdigit((unsigned char)*matcher->cursor)) { value = value * 10 + (*matcher->cursor++ - '0'); if (value > 1000) { matcher->error = "repetition count exceeds 1000"; return 0; } }
    return value;
}

static int rx_concat(RegexMatcher* matcher) {
    int* parts = NULL; size_t count = 0, capacity = 0;
    while (*matcher->cursor && *matcher->cursor != ')' && *matcher->cursor != '|' && !matcher->error) {
        int child = rx_atom(matcher);
        char quantifier = *matcher->cursor;
        if (!matcher->error && (quantifier == '*' || quantifier == '+' || quantifier == '?' || quantifier == '{')) {
            RxNode repeat = {0}; repeat.op = RX_REPEAT; repeat.left = child;
            ++matcher->cursor; repeat.minimum = quantifier == '+' ? 1 : 0; repeat.maximum = quantifier == '?' ? 1 : -1;
            if (quantifier == '{') {
                repeat.minimum = rx_number(matcher); repeat.maximum = repeat.minimum;
                if (*matcher->cursor == ',') { ++matcher->cursor; repeat.maximum = *matcher->cursor == '}' ? -1 : rx_number(matcher); }
                if (matcher->error || *matcher->cursor != '}' || (repeat.maximum >= 0 && repeat.maximum < repeat.minimum)) matcher->error = "invalid repetition range";
                else ++matcher->cursor;
            }
            if (*matcher->cursor == '?') { repeat.lazy = true; ++matcher->cursor; }
            child = matcher->error ? -1 : rx_node(matcher, repeat);
        }
        if (child < 0) break;
        if (count == capacity) { capacity = capacity ? capacity * 2 : 16; parts = (int*)rgx_realloc_array(parts, capacity, sizeof(int)); }
        parts[count++] = child;
    }
    while (count > 1) {
        size_t remaining = 0;
        for (size_t index = 0; index < count; index += 2) {
            if (index + 1 == count) parts[remaining++] = parts[index];
            else { RxNode concat = {0}; concat.op = RX_CONCAT; concat.left = parts[index]; concat.right = parts[index + 1]; parts[remaining++] = rx_node(matcher, concat); }
        }
        count = remaining;
    }
    int result = count ? parts[0] : rx_node(matcher, (RxNode){0}); free(parts); return result;
}

static int rx_expression(RegexMatcher* matcher) {
    int result = rx_concat(matcher);
    while (*matcher->cursor == '|' && !matcher->error) { ++matcher->cursor; RxNode alternate = {0}; alternate.op = RX_ALT; alternate.left = result; alternate.right = rx_concat(matcher); result = rx_node(matcher, alternate); }
    return result;
}

static int rx_emit(RegexMatcher* matcher, RxOp op, int next, int alternate, int node) {
    if (matcher->code_count == RX_LIMIT) { matcher->error = "compiled regex exceeds state limit"; return -1; }
    if (matcher->code_count == matcher->code_capacity) { matcher->code_capacity = matcher->code_capacity ? matcher->code_capacity * 2 : 32; matcher->code = (RxInstruction*)rgx_realloc_array(matcher->code, matcher->code_capacity, sizeof(RxInstruction)); }
    matcher->code[matcher->code_count] = (RxInstruction){op, next, alternate, node}; return (int)matcher->code_count++;
}

static int rx_compile(RegexMatcher* matcher, int index, int next, unsigned depth) {
    if (matcher->error || index < 0) return -1;
    if (depth > RX_DEPTH * 2) { matcher->error = "regex compilation depth exceeded"; return -1; }
    RxNode node = matcher->nodes[index];
    if (node.op == RX_EMPTY) return next;
    if (node.op == RX_CONCAT) { int right = rx_compile(matcher, node.right, next, depth + 1); return rx_compile(matcher, node.left, right, depth + 1); }
    if (node.op == RX_ALT) { int left = rx_compile(matcher, node.left, next, depth + 1), right = rx_compile(matcher, node.right, next, depth + 1); return rx_emit(matcher, RX_SPLIT, left, right, -1); }
    if (node.op == RX_REPEAT) {
        int head = next;
        if (node.maximum < 0) {
            int split = rx_emit(matcher, RX_SPLIT, next, next, -1);
            if (split < 0) return -1;
            int body = rx_compile(matcher, node.left, split, depth + 1);
            matcher->code[split].next = node.lazy ? next : body; matcher->code[split].alternate = node.lazy ? body : next; head = split;
        } else for (int count = node.maximum; count > node.minimum && !matcher->error; --count) { int body = rx_compile(matcher, node.left, head, depth + 1); head = rx_emit(matcher, RX_SPLIT, node.lazy ? head : body, node.lazy ? body : head, -1); }
        for (int count = 0; count < node.minimum && !matcher->error; ++count) head = rx_compile(matcher, node.left, head, depth + 1);
        return head;
    }
    return rx_emit(matcher, node.op, next, -1, index);
}

static bool rx_class_match(const RxNode* node, uint32_t rune) {
    bool matched = false;
    for (int index = 0; index < node->term_count; ++index) {
        RxTerm term = node->terms[index]; bool member;
        if (term.category == 1) member = rune < 128 ? isdigit((unsigned char)rune) != 0 : (rune_type(rune, CT_CTYPE1) & C1_DIGIT) != 0;
        else if (term.category == 2) member = rune_word(rune);
        else if (term.category == 3) member = rune < 128 ? isspace((unsigned char)rune) != 0 : (rune_type(rune, CT_CTYPE1) & C1_SPACE) != 0;
        else if (term.category) {
            member = rune_property(rune, term.category);
            if (node->ignore_case && (term.category == 5 || term.category == 6)) member = rune_property(rune,5) || rune_property(rune,6);
        } else { member = rune >= term.low && rune <= term.high; if (node->ignore_case) { uint32_t folded = rune_fold(rune); member = member || (folded >= rune_fold(term.low) && folded <= rune_fold(term.high)); } }
        matched = matched || (term.inverse ? !member : member);
    }
    return node->inverted ? !matched : matched;
}

static bool rx_add(RegexMatcher* matcher, RxThread* list, size_t* count, int instruction, size_t start, const char* text, size_t length, size_t position) {
    size_t pending = 0; matcher->stack[pending++] = (RxThread){instruction, start};
    while (pending) {
        RxThread thread = matcher->stack[--pending];
        if (thread.instruction < 0 || matcher->seen[thread.instruction]) continue;
        if (++matcher->work > RX_WORK_LIMIT || ++rgx_search_work > RX_WORK_LIMIT) { fprintf(stderr, "rgx: regex work limit exceeded\n"); rgx_had_error = rgx_match_error = true; return false; }
        matcher->seen[thread.instruction] = 1; RxInstruction state = matcher->code[thread.instruction];
        if (state.op == RX_SPLIT) { matcher->stack[pending++] = (RxThread){state.alternate, start}; matcher->stack[pending++] = (RxThread){state.next, start}; continue; }
        bool follow = false;
        if (state.op == RX_BOL) follow = position == 0 || (matcher->nodes[state.node].multiline && text[position-1]=='\n');
        else if (state.op == RX_EOL) follow = position == length || (matcher->nodes[state.node].multiline && text[position]=='\n');
        else if(state.op==RX_ABS_BOL)follow=position==0;
        else if(state.op==RX_ABS_EOL)follow=position==length;
        else if (state.op == RX_WORD || state.op == RX_NOTWORD || state.op == RX_LEFTWORD || state.op == RX_RIGHTWORD) {
            size_t width; bool left = position && rune_word(previous_rune(text, position));
            bool right = position < length && rune_word(utf8_rune(text + position, length - position, &width));
            follow = state.op == RX_WORD ? left != right : state.op == RX_NOTWORD ? left == right : state.op == RX_LEFTWORD ? !left : !right;
        } else { list[(*count)++] = thread; continue; }
        if (follow) matcher->stack[pending++] = (RxThread){state.next, start};
    }
    return true;
}

static bool RegexMatcher_FindMatch(IMatcher* base, const char* text, size_t length, size_t offset, MatchSpan* span) {
    RegexMatcher* matcher = (RegexMatcher*)base;
    if (offset > length) return false;
    size_t count = 0, position = offset; bool found = false; MatchSpan candidate = {0}; matcher->work = 0;
    memset(matcher->seen, 0, matcher->code_count);
    while (true) {
        if (!found && !rx_add(matcher, matcher->current, &count, matcher->start, position, text, length, position)) return false;
        size_t active = count;
        for (size_t index = 0; index < count; ++index) if (matcher->code[matcher->current[index].instruction].op == RX_MATCH) { candidate.start = matcher->current[index].start; candidate.length = position - candidate.start; found = true; active = index; break; }
        if (found && !active) { if (span) *span = candidate; return true; }
        if (position == length) break;
        size_t width; uint32_t rune = utf8_rune(text + position, length - position, &width); size_t next_count = 0;
        memset(matcher->seen, 0, matcher->code_count);
        for (size_t index = 0; index < active; ++index) {
            RxThread thread = matcher->current[index]; RxInstruction state = matcher->code[thread.instruction]; RxNode* node = &matcher->nodes[state.node];
            bool accepts = state.op == RX_ANY ? node->dotall || rune != '\n' : state.op == RX_CLASS ? rx_class_match(node, rune) : state.op == RX_CHAR && (node->ignore_case ? rune_fold(node->rune) == rune_fold(rune) : node->rune == rune);
            if (accepts && !rx_add(matcher, matcher->next, &next_count, state.next, thread.start, text, length, position + width)) return false;
        }
        RxThread* swap = matcher->current; matcher->current = matcher->next; matcher->next = swap; count = next_count; position += width;
        if (found && !count) break;
    }
    if (found && span) *span = candidate;
    return found;
}

static void RegexMatcher_Destroy(IMatcher* base) {
    RegexMatcher* matcher = (RegexMatcher*)base;
    if (!matcher) return;
    free(matcher->nodes); free(matcher->code); free(matcher->current); free(matcher->next); free(matcher->stack); free(matcher->seen); free(matcher);
}
static const IMatcherVTable REGEX_MATCHER_VTBL = {RegexMatcher_FindMatch, RegexMatcher_Destroy};

IMatcher* RegexMatcher_New(const char* pattern, bool ignore_case, bool word_boundary) {
    return RegexMatcher_NewMode(pattern,ignore_case,word_boundary,false,false);
}

IMatcher* RegexMatcher_NewMode(const char* pattern,bool ignore_case,bool word_boundary,bool multiline,bool dotall) {
    RegexMatcher* matcher = (RegexMatcher*)rgx_calloc(1, sizeof(RegexMatcher)); matcher->base.vptr = &REGEX_MATCHER_VTBL;
    matcher->cursor = pattern; matcher->ignore_case = ignore_case;
    matcher->multiline=multiline;matcher->dotall=dotall;
    if (strlen(pattern) > 65536) matcher->error = "pattern exceeds 64 KiB";
    int root = matcher->error ? -1 : rx_expression(matcher);
    if (!matcher->error && *matcher->cursor) matcher->error = "unexpected closing parenthesis";
    int accept = rx_emit(matcher, RX_MATCH, -1, -1, -1);
    if (word_boundary) accept = rx_emit(matcher, RX_RIGHTWORD, accept, -1, -1);
    matcher->start = rx_compile(matcher, root, accept, 0);
    if (word_boundary) matcher->start = rx_emit(matcher, RX_LEFTWORD, matcher->start, -1, -1);
    if (matcher->error) { fprintf(stderr, "rgx: regex error: %s\n", matcher->error); rgx_had_error = true; RegexMatcher_Destroy((IMatcher*)matcher); return NULL; }
    matcher->current = (RxThread*)rgx_calloc(matcher->code_count, sizeof(RxThread)); matcher->next = (RxThread*)rgx_calloc(matcher->code_count, sizeof(RxThread));
    matcher->stack = (RxThread*)rgx_calloc(matcher->code_count * 4 + 1, sizeof(RxThread)); matcher->seen = (unsigned char*)rgx_calloc(matcher->code_count, 1);
    return (IMatcher*)matcher;
}
size_t regex_matcher_node_count(const IMatcher* matcher) {
    return ((const RegexMatcher*)matcher)->node_count;
}

size_t regex_matcher_state_count(const IMatcher* matcher) {
    return ((const RegexMatcher*)matcher)->code_count;
}
