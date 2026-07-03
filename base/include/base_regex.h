#ifndef BASE_REGEX_H
#define BASE_REGEX_H

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// Small backtracking regex engine.
//
// Supported:
//   literals and escapes (\. \* \\ ...)
//   . (any char except '\n')
//   * + ? (greedy)
//   [abc] [^abc] [a-z] with common escapes inside (\d \w \s)
//   \d \D \w \W \s \S
//   ( ... ) capturing groups (up to REGEX_MAX_GROUPS)
//   | alternation
//
// Not supported: anchors (^ $), {n,m}, lookaround, backrefs inside a pattern.
// ---------------------------------------------------------------------------

#define REGEX_MAX_GROUPS 16

typedef struct Regex Regex;

typedef struct Regex_Group Regex_Group;
struct Regex_Group
{
    U64 start;
    U64 end;
};

typedef struct Regex_Match Regex_Match;
struct Regex_Match
{
    U64 start;
    U64 end;
    U32 group_count;
    Regex_Group groups[REGEX_MAX_GROUPS];
};

// Returns 0 on parse error; if err_msg is non-null, it receives a static string.
Regex *Regex_Compile(Arena *a, String8 pattern, String8 *err_msg);

// Find the first match at-or-after `start`. Returns 1 on success.
B32 Regex_Find(Regex *re, String8 input, U64 start, Regex_Match *out);

#endif // BASE_REGEX_H
