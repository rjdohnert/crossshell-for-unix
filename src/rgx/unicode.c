#include "unicode.h"

static inline bool is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

uint32_t utf8_rune(const char* text, size_t length, size_t* width) {
    unsigned char first = (unsigned char)text[0];
    *width = 1;
    if (first < 128) return first;
    size_t count = first >= 0xC2 && first <= 0xDF ? 2 : first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
    if (!count || length < count) return 0x110000U + first;
    uint32_t value = first & (count == 2 ? 31U : count == 3 ? 15U : 7U);
    for (size_t index = 1; index < count; ++index) {
        unsigned char next = (unsigned char)text[index];
        if ((next & 0xC0) != 0x80) return 0x110000U + first;
        value = (value << 6) | (next & 63U);
    }
    if ((count == 3 && value < 0x800) || (count == 4 && value < 0x10000) || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return 0x110000U + first;
    *width = count;
    return value;
}

WORD rune_type(uint32_t rune, DWORD type) {
    if (rune > 0x10FFFF) return 0;
    wchar_t chars[2]; WORD info[2] = {0, 0}; int count = 1;
    if (rune > 0xFFFF) { chars[0] = (wchar_t)(0xD800 + ((rune - 0x10000) >> 10)); chars[1] = (wchar_t)(0xDC00 + ((rune - 0x10000) & 1023)); count = 2; }
    else chars[0] = (wchar_t)rune;
    if (!GetStringTypeW(type, chars, count, info)) return 0;
    return info[0] | info[1];
}

bool rune_word(uint32_t rune) {
    if (rune < 128) return is_word_char((char)rune);
    return rune_property(rune, 4) || (rune_type(rune, CT_CTYPE1) & C1_DIGIT) != 0 ||
        (rune_type(rune, CT_CTYPE3) & (C3_NONSPACING | C3_DIACRITIC)) != 0 ||
        rune == 0x203F || rune == 0x2040 || rune == 0x200C || rune == 0x200D;
}

uint32_t rune_fold(uint32_t rune) {
    if (rune < 128) return (uint32_t)tolower((unsigned char)rune);
    if (rune > 0x10FFFF) return rune;
    if (rune == 0x03C2) return 0x03C3;
    if (rune == 0x017F) return 's';
    if (rune == 0x212A) return 'k';
    wchar_t input[2], output[4];
    int count = 1;
    if (rune > 0xFFFF) {
        input[0] = (wchar_t)(0xD800 + ((rune - 0x10000) >> 10));
        input[1] = (wchar_t)(0xDC00 + ((rune - 0x10000) & 1023));
        count = 2;
    } else input[0] = (wchar_t)rune;
    int mapped = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, input, count, output, 4, NULL, NULL, 0);
    if (mapped == 1) return (uint32_t)output[0];
    if (mapped == 2 && output[0] >= 0xD800 && output[0] <= 0xDBFF && output[1] >= 0xDC00 && output[1] <= 0xDFFF)
        return 0x10000 + (((uint32_t)output[0] - 0xD800) << 10) + ((uint32_t)output[1] - 0xDC00);
    return rune;
}

int unicode_property_id(const char* name) {
    static const struct { const char* name; int id; } properties[] = {
        {"L",4},{"Letter",4},{"Alphabetic",4},{"Lu",5},{"Uppercase_Letter",5},
        {"Ll",6},{"Lowercase_Letter",6},{"P",7},{"Punctuation",7},{"M",8},{"Mark",8},
        {"Greek",9},{"Script=Greek",9},{"Latin",10},{"Script=Latin",10},
        {"Cyrillic",11},{"Script=Cyrillic",11},{"Han",12},{"Script=Han",12},
        {"ASCII",13},{"Nd",1},{"Decimal_Number",1},
        {"White_Space",3},{"Space",3},{"Word",2}
    };
    for (size_t index=0; index<sizeof(properties)/sizeof(properties[0]); ++index)
        if (strcmp(name,properties[index].name)==0) return properties[index].id;
    return 0;
}

bool rune_property(uint32_t rune, int property) {
    WORD type = rune_type(rune, CT_CTYPE1);
    switch (property) {
        case 1: return (type & C1_DIGIT) != 0;
        case 2: return rune_word(rune);
        case 3: return (type & C1_SPACE) != 0;
        case 4: return (type & C1_ALPHA) != 0 || (rune >= 0x10400 && rune <= 0x1044F) ||
            (rune >= 0x104B0 && rune <= 0x104FB) || (rune >= 0x20000 && rune <= 0x323AF);
        case 5: return (type & C1_UPPER) != 0 || (rune >= 0x10400 && rune <= 0x10427) || (rune >= 0x104B0 && rune <= 0x104D3);
        case 6: return (type & C1_LOWER) != 0 || (rune >= 0x10428 && rune <= 0x1044F) || (rune >= 0x104D8 && rune <= 0x104FB);
        case 7: return (type & C1_PUNCT) != 0;
        case 8: return (rune_type(rune, CT_CTYPE3) & (C3_NONSPACING | C3_DIACRITIC)) != 0;
        case 9: return (rune >= 0x0370 && rune <= 0x03FF) || (rune >= 0x1F00 && rune <= 0x1FFF);
        case 10: return (rune >= 'A' && rune <= 'Z') || (rune >= 'a' && rune <= 'z') ||
            (rune >= 0x00C0 && rune <= 0x024F) || (rune >= 0x1E00 && rune <= 0x1EFF);
        case 11: return (rune >= 0x0400 && rune <= 0x052F) || (rune >= 0x2DE0 && rune <= 0x2DFF) || (rune >= 0xA640 && rune <= 0xA69F);
        case 12: return (rune >= 0x3400 && rune <= 0x4DBF) || (rune >= 0x4E00 && rune <= 0x9FFF) || (rune >= 0x20000 && rune <= 0x323AF);
        case 13: return rune < 128;
        default: return false;
    }
}

bool utf8_valid(const char* text, size_t length) {
    for (size_t position=0; position<length;) {
        size_t width;
        if (utf8_rune(text+position,length-position,&width)>0x10FFFF) return false;
        position+=width;
    }
    return true;
}

uint32_t previous_rune(const char* text, size_t position) {
    if (!position) return 0;
    size_t begin = position - 1;
    while (begin && ((unsigned char)text[begin] & 0xC0) == 0x80 && position - begin < 4) --begin;
    size_t width; return utf8_rune(text + begin, position - begin, &width);
}
