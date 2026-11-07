#ifndef CSPLIT_REGEX_ENGINE_H
#define CSPLIT_REGEX_ENGINE_H

typedef struct CompiledRegex CompiledRegex;

/* Compile the supported byte-oriented regex syntax; NULL means invalid/OOM. */
CompiledRegex* RegexEngine_compile(const char* pattern);

/* Return 1 for a match, 0 for no match, or -1 if matching cannot allocate. */
int RegexEngine_matches(const CompiledRegex* regex, const char* text);

/* Free a compiled regular expression. */
void RegexEngine_destroy(CompiledRegex* regex);

#endif
