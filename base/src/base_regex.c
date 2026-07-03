#include "base_regex.h"

#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------

typedef U32 Node_Kind;
enum
{
    Node_Char,
    Node_Any,
    Node_Class,
    Node_Group,
    Node_Star,
    Node_Plus,
    Node_Opt,
    Node_Concat,
    Node_Alt,
};

typedef struct Node Node;
struct Node
{
    Node_Kind kind;
    U8 ch;
    U8 cls[32]; // bitmap for Node_Class
    U32 group_idx;
    Node *child;     // unary
    Node *left;      // binary
    Node *right;     // binary
    Node **children; // concat
    U32 child_count;
};

// ---------------------------------------------------------------------------
// Bytecode
// ---------------------------------------------------------------------------

typedef U8 RegexOp;
enum
{
    RegexOp_Match,
    RegexOp_Char,
    RegexOp_Any,
    RegexOp_Class,
    RegexOp_Jmp,
    RegexOp_Split,
    RegexOp_Save,
};

typedef struct RegexInst RegexInst;
struct RegexInst
{
    RegexOp op;
    U32 a;
    U32 b;
};

struct Regex
{
    RegexInst *prog;
    U32 prog_len;
    U8 *cls;
    U32 cls_count;
    U32 group_count;
};

// ---------------------------------------------------------------------------
// Compiler context
// ---------------------------------------------------------------------------

typedef struct Compiler Compiler;
struct Compiler
{
    Arena *arena;
    String8 pattern;
    U64 pos;
    U32 group_idx;
    String8 err;
};

static Node *NodeAlloc(Compiler *c, Node_Kind kind)
{
    Node *n = Arena_PushStruct(c->arena, Node);
    n->kind = kind;
    return n;
}

static void ClsAdd(U8 *cls, U8 c)
{
    cls[c >> 3] |= (U8)(1u << (c & 7));
}

static void ClsAddRange(U8 *cls, U8 lo, U8 hi)
{
    for (U32 c = lo; c <= hi; c++)
    {
        ClsAdd(cls, (U8)c);
    }
}

static void ClsNegate(U8 *cls)
{
    for (U32 i = 0; i < 32; i++)
    {
        cls[i] = (U8)~cls[i];
    }
    // Exclude NUL and newline from negated class? Traditional regex semantics: [^a] matches
    // any byte except 'a', including newlines. Leave as-is.
}

static void ClsApplyEscape(U8 *cls, U8 esc)
{
    switch (esc)
    {
    case 'd':
        ClsAddRange(cls, '0', '9');
        break;
    case 'D': {
        U8 tmp[32] = {0};
        ClsAddRange(tmp, '0', '9');
        for (U32 i = 0; i < 32; i++)
        {
            cls[i] |= (U8)~tmp[i];
        }
        break;
    }
    case 'w':
        ClsAddRange(cls, 'a', 'z');
        ClsAddRange(cls, 'A', 'Z');
        ClsAddRange(cls, '0', '9');
        ClsAdd(cls, '_');
        break;
    case 'W': {
        U8 tmp[32] = {0};
        ClsAddRange(tmp, 'a', 'z');
        ClsAddRange(tmp, 'A', 'Z');
        ClsAddRange(tmp, '0', '9');
        ClsAdd(tmp, '_');
        for (U32 i = 0; i < 32; i++)
        {
            cls[i] |= (U8)~tmp[i];
        }
        break;
    }
    case 's':
        ClsAdd(cls, ' ');
        ClsAdd(cls, '\t');
        ClsAdd(cls, '\n');
        ClsAdd(cls, '\r');
        ClsAdd(cls, '\f');
        ClsAdd(cls, '\v');
        break;
    case 'S': {
        U8 tmp[32] = {0};
        ClsAdd(tmp, ' ');
        ClsAdd(tmp, '\t');
        ClsAdd(tmp, '\n');
        ClsAdd(tmp, '\r');
        ClsAdd(tmp, '\f');
        ClsAdd(tmp, '\v');
        for (U32 i = 0; i < 32; i++)
        {
            cls[i] |= (U8)~tmp[i];
        }
        break;
    }
    case 'n':
        ClsAdd(cls, '\n');
        break;
    case 't':
        ClsAdd(cls, '\t');
        break;
    case 'r':
        ClsAdd(cls, '\r');
        break;
    case 'f':
        ClsAdd(cls, '\f');
        break;
    case 'v':
        ClsAdd(cls, '\v');
        break;
    default:
        ClsAdd(cls, esc);
        break;
    }
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

static Node *ParseAlt(Compiler *c);

static Node *ParseClassNode(Compiler *c)
{
    Node *n = NodeAlloc(c, Node_Class);
    c->pos++; // consume '['
    B32 negate = 0;
    if (c->pos < c->pattern.len && c->pattern.data[c->pos] == '^')
    {
        negate = 1;
        c->pos++;
    }
    while (c->pos < c->pattern.len && c->pattern.data[c->pos] != ']')
    {
        U8 ch = c->pattern.data[c->pos++];
        if (ch == '\\' && c->pos < c->pattern.len)
        {
            U8 esc = c->pattern.data[c->pos++];
            ClsApplyEscape(n->cls, esc);
            continue;
        }
        if (c->pos + 1 < c->pattern.len && c->pattern.data[c->pos] == '-' && c->pattern.data[c->pos + 1] != ']')
        {
            c->pos++; // consume '-'
            U8 hi = c->pattern.data[c->pos++];
            if (hi == '\\' && c->pos < c->pattern.len)
            {
                hi = c->pattern.data[c->pos++];
            }
            if (ch <= hi)
            {
                ClsAddRange(n->cls, ch, hi);
            }
            continue;
        }
        ClsAdd(n->cls, ch);
    }
    if (c->pos >= c->pattern.len)
    {
        c->err = String8_Lit("unterminated character class");
        return n;
    }
    c->pos++; // consume ']'
    if (negate)
    {
        ClsNegate(n->cls);
    }
    return n;
}

static Node *ParseAtom(Compiler *c)
{
    if (c->pos >= c->pattern.len)
    {
        c->err = String8_Lit("unexpected end of pattern");
        return NodeAlloc(c, Node_Char);
    }
    U8 ch = c->pattern.data[c->pos];
    if (ch == '(')
    {
        c->pos++;
        Node *g = NodeAlloc(c, Node_Group);
        g->group_idx = c->group_idx++;
        if (g->group_idx >= REGEX_MAX_GROUPS)
        {
            c->err = String8_Lit("too many groups");
            return g;
        }
        g->child = ParseAlt(c);
        if (c->pos >= c->pattern.len || c->pattern.data[c->pos] != ')')
        {
            c->err = String8_Lit("missing ')'");
            return g;
        }
        c->pos++;
        return g;
    }
    if (ch == '[')
    {
        return ParseClassNode(c);
    }
    if (ch == '.')
    {
        c->pos++;
        return NodeAlloc(c, Node_Any);
    }
    if (ch == '\\')
    {
        c->pos++;
        if (c->pos >= c->pattern.len)
        {
            c->err = String8_Lit("trailing backslash");
            return NodeAlloc(c, Node_Char);
        }
        U8 esc = c->pattern.data[c->pos++];
        if (esc == 'd' || esc == 'D' || esc == 'w' || esc == 'W' || esc == 's' || esc == 'S')
        {
            Node *n = NodeAlloc(c, Node_Class);
            ClsApplyEscape(n->cls, esc);
            return n;
        }
        // Mapped escapes
        U8 mapped = esc;
        switch (esc)
        {
        case 'n':
            mapped = '\n';
            break;
        case 't':
            mapped = '\t';
            break;
        case 'r':
            mapped = '\r';
            break;
        case 'f':
            mapped = '\f';
            break;
        case 'v':
            mapped = '\v';
            break;
        case '0':
            mapped = 0;
            break;
        default:
            mapped = esc;
            break;
        }
        Node *n = NodeAlloc(c, Node_Char);
        n->ch = mapped;
        return n;
    }
    // Metachars that shouldn't appear here
    if (ch == ')' || ch == '|' || ch == '*' || ch == '+' || ch == '?')
    {
        c->err = String8_Lit("unexpected metacharacter");
        return NodeAlloc(c, Node_Char);
    }
    c->pos++;
    Node *n = NodeAlloc(c, Node_Char);
    n->ch = ch;
    return n;
}

static Node *ParseTerm(Compiler *c)
{
    Node *atom = ParseAtom(c);
    if (c->pos >= c->pattern.len)
    {
        return atom;
    }
    U8 q = c->pattern.data[c->pos];
    if (q == '*' || q == '+' || q == '?')
    {
        c->pos++;
        Node_Kind k = q == '*' ? Node_Star : (q == '+' ? Node_Plus : Node_Opt);
        Node *wrap = NodeAlloc(c, k);
        wrap->child = atom;
        return wrap;
    }
    return atom;
}

static Node *ParseConcat(Compiler *c)
{
    U32 cap = 8;
    Node **items = Arena_PushArrayNoZero(c->arena, Node *, cap);
    U32 count = 0;
    while (c->pos < c->pattern.len)
    {
        U8 ch = c->pattern.data[c->pos];
        if (ch == '|' || ch == ')')
        {
            break;
        }
        if (count == cap)
        {
            U32 new_cap = cap * 2;
            Node **new_items = Arena_PushArrayNoZero(c->arena, Node *, new_cap);
            memcpy(new_items, items, cap * sizeof(Node *));
            items = new_items;
            cap = new_cap;
        }
        items[count++] = ParseTerm(c);
        if (c->err.len != 0)
        {
            break;
        }
    }
    if (count == 1)
    {
        return items[0];
    }
    Node *n = NodeAlloc(c, Node_Concat);
    n->children = items;
    n->child_count = count;
    return n;
}

static Node *ParseAlt(Compiler *c)
{
    Node *left = ParseConcat(c);
    if (c->pos < c->pattern.len && c->pattern.data[c->pos] == '|')
    {
        c->pos++;
        Node *right = ParseAlt(c);
        Node *n = NodeAlloc(c, Node_Alt);
        n->left = left;
        n->right = right;
        return n;
    }
    return left;
}

// ---------------------------------------------------------------------------
// Emitter
// ---------------------------------------------------------------------------

typedef struct Emitter Emitter;
struct Emitter
{
    Arena *arena;
    RegexInst *prog;
    U32 prog_len;
    U32 prog_cap;
    U8 *cls;
    U32 cls_count;
    U32 cls_cap;
};

static U32 Emit(Emitter *e, RegexOp op, U32 a, U32 b)
{
    if (e->prog_len == e->prog_cap)
    {
        U32 new_cap = e->prog_cap == 0 ? 32 : e->prog_cap * 2;
        RegexInst *new_prog = Arena_PushArrayNoZero(e->arena, RegexInst, new_cap);
        memcpy(new_prog, e->prog, e->prog_len * sizeof(RegexInst));
        e->prog = new_prog;
        e->prog_cap = new_cap;
    }
    U32 idx = e->prog_len++;
    e->prog[idx].op = op;
    e->prog[idx].a = a;
    e->prog[idx].b = b;
    return idx;
}

static U32 AddClass(Emitter *e, U8 *cls)
{
    if (e->cls_count == e->cls_cap)
    {
        U32 new_cap = e->cls_cap == 0 ? 4 : e->cls_cap * 2;
        U8 *new_cls = Arena_PushArrayNoZero(e->arena, U8, new_cap * 32);
        memcpy(new_cls, e->cls, e->cls_count * 32);
        e->cls = new_cls;
        e->cls_cap = new_cap;
    }
    U32 idx = e->cls_count++;
    memcpy(&e->cls[idx * 32], cls, 32);
    return idx;
}

static void EmitNode(Emitter *e, Node *n)
{
    switch (n->kind)
    {
    case Node_Char:
        Emit(e, RegexOp_Char, n->ch, 0);
        break;
    case Node_Any:
        Emit(e, RegexOp_Any, 0, 0);
        break;
    case Node_Class: {
        U32 idx = AddClass(e, n->cls);
        Emit(e, RegexOp_Class, idx, 0);
        break;
    }
    case Node_Group:
        Emit(e, RegexOp_Save, n->group_idx * 2, 0);
        EmitNode(e, n->child);
        Emit(e, RegexOp_Save, n->group_idx * 2 + 1, 0);
        break;
    case Node_Concat:
        for (U32 i = 0; i < n->child_count; i++)
        {
            EmitNode(e, n->children[i]);
        }
        break;
    case Node_Star: {
        // L1: SPLIT L2, L3
        // L2: <child>
        //     JMP L1
        // L3:
        U32 split_pc = Emit(e, RegexOp_Split, 0, 0);
        U32 body_start = e->prog_len;
        EmitNode(e, n->child);
        Emit(e, RegexOp_Jmp, split_pc, 0);
        U32 after = e->prog_len;
        e->prog[split_pc].a = body_start;
        e->prog[split_pc].b = after;
        break;
    }
    case Node_Plus: {
        // L1: <child>
        //     SPLIT L1, L2
        // L2:
        U32 body_start = e->prog_len;
        EmitNode(e, n->child);
        U32 split_pc = Emit(e, RegexOp_Split, body_start, 0);
        e->prog[split_pc].b = e->prog_len;
        break;
    }
    case Node_Opt: {
        // SPLIT L1, L2
        // L1: <child>
        // L2:
        U32 split_pc = Emit(e, RegexOp_Split, 0, 0);
        U32 body_start = e->prog_len;
        EmitNode(e, n->child);
        e->prog[split_pc].a = body_start;
        e->prog[split_pc].b = e->prog_len;
        break;
    }
    case Node_Alt: {
        // SPLIT L1, L2
        // L1: <left>
        //     JMP L3
        // L2: <right>
        // L3:
        U32 split_pc = Emit(e, RegexOp_Split, 0, 0);
        U32 left_start = e->prog_len;
        EmitNode(e, n->left);
        U32 jmp_pc = Emit(e, RegexOp_Jmp, 0, 0);
        U32 right_start = e->prog_len;
        EmitNode(e, n->right);
        U32 end = e->prog_len;
        e->prog[split_pc].a = left_start;
        e->prog[split_pc].b = right_start;
        e->prog[jmp_pc].a = end;
        break;
    }
    }
}

Regex *Regex_Compile(Arena *a, String8 pattern, String8 *err_msg)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    Compiler c = {0};
    c.arena = scratch.arena;
    c.pattern = pattern;

    Node *root = ParseAlt(&c);
    if (c.err.len == 0 && c.pos != pattern.len)
    {
        c.err = String8_Lit("unexpected trailing characters");
    }
    if (c.err.len != 0)
    {
        if (err_msg != 0)
        {
            *err_msg = c.err;
        }
        Scratch_End(scratch);
        return 0;
    }

    Emitter e = {0};
    e.arena = a;
    EmitNode(&e, root);
    Emit(&e, RegexOp_Match, 0, 0);

    Regex *re = Arena_PushStruct(a, Regex);
    re->prog = e.prog;
    re->prog_len = e.prog_len;
    re->cls = e.cls;
    re->cls_count = e.cls_count;
    re->group_count = c.group_idx;
    Scratch_End(scratch);
    return re;
}

// ---------------------------------------------------------------------------
// Executor
// ---------------------------------------------------------------------------

static B32 ExecAt(Regex *re, String8 input, U32 pc, U64 sp, U64 *saves, U64 *out_end)
{
    for (;;)
    {
        RegexInst inst = re->prog[pc];
        switch (inst.op)
        {
        case RegexOp_Match:
            *out_end = sp;
            return 1;
        case RegexOp_Char:
            if (sp >= input.len || input.data[sp] != (U8)inst.a)
            {
                return 0;
            }
            pc++;
            sp++;
            break;
        case RegexOp_Any:
            if (sp >= input.len || input.data[sp] == '\n')
            {
                return 0;
            }
            pc++;
            sp++;
            break;
        case RegexOp_Class: {
            if (sp >= input.len)
            {
                return 0;
            }
            U8 c = input.data[sp];
            U8 *bmp = &re->cls[inst.a * 32];
            if ((bmp[c >> 3] & (1u << (c & 7))) == 0)
            {
                return 0;
            }
            pc++;
            sp++;
            break;
        }
        case RegexOp_Jmp:
            pc = inst.a;
            break;
        case RegexOp_Split: {
            U64 saves_copy[REGEX_MAX_GROUPS * 2];
            memcpy(saves_copy, saves, sizeof(saves_copy));
            U64 tmp_end;
            if (ExecAt(re, input, inst.a, sp, saves, &tmp_end))
            {
                *out_end = tmp_end;
                return 1;
            }
            memcpy(saves, saves_copy, sizeof(saves_copy));
            pc = inst.b;
            break;
        }
        case RegexOp_Save:
            saves[inst.a] = sp;
            pc++;
            break;
        }
    }
}

B32 Regex_Find(Regex *re, String8 input, U64 start, Regex_Match *out)
{
    for (U64 sp = start; sp <= input.len; sp++)
    {
        U64 saves[REGEX_MAX_GROUPS * 2];
        for (U32 i = 0; i < REGEX_MAX_GROUPS * 2; i++)
        {
            saves[i] = (U64)-1;
        }
        U64 end_sp;
        if (ExecAt(re, input, 0, sp, saves, &end_sp))
        {
            out->start = sp;
            out->end = end_sp;
            out->group_count = re->group_count;
            for (U32 g = 0; g < re->group_count && g < REGEX_MAX_GROUPS; g++)
            {
                out->groups[g].start = saves[g * 2];
                out->groups[g].end = saves[g * 2 + 1];
            }
            return 1;
        }
    }
    return 0;
}
