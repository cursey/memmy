// ===========================================================================
// AVL Tree tests (intrusive)
// ===========================================================================

#include <string.h>

#include "base_arena.h"
#include "base_avl.h"
#include "base_core.h"
#include "base_hash.h"
#include "base_string.h"
#include "test_framework.h"

static B32 avl_test__ok;

static I32 AvlTest_CheckBalance(AvlLink *n)
{
    if (n == 0)
    {
        return -1;
    }
    I32 lh = AvlTest_CheckBalance(n->left);
    I32 rh = AvlTest_CheckBalance(n->right);
    I32 bf = rh - lh;
    if (bf < -1 || bf > 1)
    {
        avl_test__ok = 0;
    }
    I32 h = 1 + (lh > rh ? lh : rh);
    if (n->height != h)
    {
        avl_test__ok = 0;
    }
    return h;
}

static void AvlTest_AssertBalanced(AvlLink *root)
{
    avl_test__ok = 1;
    AvlTest_CheckBalance(root);
    AssertTrue(avl_test__ok);
}

// ===========================================================================
// Suite 1: U64-keyed, no free list
// ===========================================================================

typedef struct AvlU64Node AvlU64Node;
struct AvlU64Node
{
    AvlLink link;
    U64 key;
    U64 value;
};

typedef struct AvlU64Tree AvlU64Tree;
struct AvlU64Tree
{
    AvlTree tree; // AvlU64Node
    Arena *arena;
};

static AvlU64Tree AvlU64Tree_Create(Arena *a)
{
    AvlU64Tree tt = {0};
    tt.arena = a;
    return tt;
}

static I32 AvlU64_Cmp(void *link, void *ctx)
{
    AvlU64Node *e = ContainerOf(link, AvlU64Node, link);
    U64 key = *(U64 *)ctx;
    if (e->key < key)
        return -1;
    if (e->key > key)
        return 1;
    return 0;
}

static AvlU64Node *AvlU64Tree_Find(AvlU64Tree *tt, U64 key)
{
    AvlLink *found = Avl_Find(&tt->tree, AvlU64_Cmp, &key);
    if (found == 0)
    {
        return 0;
    }
    return ContainerOf(found, AvlU64Node, link);
}

static AvlU64Node *AvlU64Tree_Insert(AvlU64Tree *tt, U64 key, U64 value)
{
    AvlLink **slot;
    AvlLink *parent = Avl_FindInsertLocation(&tt->tree, AvlU64_Cmp, &key, &slot);

    if (*slot != 0)
    {
        AvlU64Node *e = ContainerOf(*slot, AvlU64Node, link);
        e->value = value;
        return e;
    }

    AvlU64Node *node = Arena_PushStruct(tt->arena, AvlU64Node);
    node->key = key;
    node->value = value;
    Avl_Insert(&tt->tree, parent, slot, &node->link);
    return node;
}

static void AvlU64Tree_Remove(AvlU64Tree *tt, U64 key)
{
    AvlU64Node *e = AvlU64Tree_Find(tt, key);
    if (e == 0)
    {
        return;
    }
    Avl_Remove(&tt->tree, &e->link);
}

// ---------------------------------------------------------------------------
// Suite 1 tests
// ---------------------------------------------------------------------------

Test(Test_AvlU64InsertFind)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    U64 vals[] = {10, 20, 30, 40, 50};
    for (U64 i = 0; i < ArrayCount(vals); i++)
    {
        AvlU64Tree_Insert(&tt, vals[i], vals[i] * 10);
    }

    for (U64 i = 0; i < ArrayCount(vals); i++)
    {
        AvlU64Node *e = AvlU64Tree_Find(&tt, vals[i]);
        AssertTrue(e != 0);
        AssertEq(e->key, vals[i]);
        AssertEq(e->value, vals[i] * 10);
    }

    AssertTrue(AvlU64Tree_Find(&tt, 99) == 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64InsertOverwrite)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 42, 100);
    AssertEq(tt.tree.count, 1);

    AvlU64Tree_Insert(&tt, 42, 200);
    AssertEq(tt.tree.count, 1);

    AvlU64Node *e = AvlU64Tree_Find(&tt, 42);
    AssertTrue(e != 0);
    AssertEq(e->value, 200);

    Scratch_End(scratch);
}

Test(Test_AvlU64Remove)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 10, 100);
    AvlU64Tree_Insert(&tt, 20, 200);
    AvlU64Tree_Insert(&tt, 30, 300);

    AvlU64Tree_Remove(&tt, 20);

    AssertTrue(AvlU64Tree_Find(&tt, 10) != 0);
    AssertTrue(AvlU64Tree_Find(&tt, 20) == 0);
    AssertTrue(AvlU64Tree_Find(&tt, 30) != 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64RemoveNonexistent)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 10, 100);
    AvlU64Tree_Remove(&tt, 99);

    AssertEq(tt.tree.count, 1);
    AssertTrue(AvlU64Tree_Find(&tt, 10) != 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64Ordering)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    U64 vals[] = {50, 30, 70, 10, 40, 60, 80, 20};
    for (U64 i = 0; i < ArrayCount(vals); i++)
    {
        AvlU64Tree_Insert(&tt, vals[i], 0);
    }

    U64 expected[] = {10, 20, 30, 40, 50, 60, 70, 80};
    U64 idx = 0;
    Avl_ForEach(AvlU64Node, e, &tt.tree, link)
    {
        AssertTrue(idx < ArrayCount(expected));
        AssertEq(e->key, expected[idx]);
        idx++;
    }
    AssertEq(idx, ArrayCount(expected));

    Scratch_End(scratch);
}

Test(Test_AvlU64ReverseOrdering)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    U64 vals[] = {50, 30, 70, 10, 40, 60, 80, 20};
    for (U64 i = 0; i < ArrayCount(vals); i++)
    {
        AvlU64Tree_Insert(&tt, vals[i], 0);
    }

    U64 expected[] = {80, 70, 60, 50, 40, 30, 20, 10};
    U64 idx = 0;
    Avl_ForEachReverse(AvlU64Node, e, &tt.tree, link)
    {
        AssertTrue(idx < ArrayCount(expected));
        AssertEq(e->key, expected[idx]);
        idx++;
    }
    AssertEq(idx, ArrayCount(expected));

    Scratch_End(scratch);
}

Test(Test_AvlU64BalanceAscending)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    for (U64 i = 1; i <= 100; i++)
    {
        AvlU64Tree_Insert(&tt, i, 0);
    }

    for (U64 i = 1; i <= 100; i++)
    {
        AssertTrue(AvlU64Tree_Find(&tt, i) != 0);
    }

    AvlTest_AssertBalanced(tt.tree.root);
    AssertTrue(tt.tree.root->height <= 11);

    Scratch_End(scratch);
}

Test(Test_AvlU64BalanceDescending)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    for (U64 i = 100; i >= 1; i--)
    {
        AvlU64Tree_Insert(&tt, i, 0);
    }

    for (U64 i = 1; i <= 100; i++)
    {
        AssertTrue(AvlU64Tree_Find(&tt, i) != 0);
    }

    AvlTest_AssertBalanced(tt.tree.root);
    AssertTrue(tt.tree.root->height <= 11);

    Scratch_End(scratch);
}

Test(Test_AvlU64Large)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    U64 n = 10000;
    for (U64 i = 0; i < n; i++)
    {
        AvlU64Tree_Insert(&tt, i, i * 10);
    }
    AssertEq(tt.tree.count, n);

    for (U64 i = 0; i < n; i++)
    {
        AssertTrue(AvlU64Tree_Find(&tt, i) != 0);
    }

    for (U64 i = 0; i < n; i += 2)
    {
        AvlU64Tree_Remove(&tt, i);
    }
    AssertEq(tt.tree.count, n / 2);

    for (U64 i = 0; i < n; i++)
    {
        AvlU64Node *e = AvlU64Tree_Find(&tt, i);
        if (i % 2 == 0)
        {
            AssertTrue(e == 0);
        }
        else
        {
            AssertTrue(e != 0);
        }
    }

    AvlTest_AssertBalanced(tt.tree.root);

    Scratch_End(scratch);
}

Test(Test_AvlU64Empty)
{
    AvlU64Tree tt = {0};

    AssertTrue(AvlU64Tree_Find(&tt, 1) == 0);
    AssertTrue(Avl_Min(&tt.tree) == 0);
    AssertTrue(Avl_Max(&tt.tree) == 0);

    AvlU64Tree_Remove(&tt, 1);
    AssertEq(tt.tree.count, 0);
}

Test(Test_AvlU64Single)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 42, 100);
    AssertEq(tt.tree.count, 1);

    AvlU64Node *e = AvlU64Tree_Find(&tt, 42);
    AssertTrue(e != 0);
    AssertEq(e->value, 100);

    AssertTrue(Avl_Min(&tt.tree) == &e->link);
    AssertTrue(Avl_Max(&tt.tree) == &e->link);
    AssertTrue(Avl_Next(&e->link) == 0);
    AssertTrue(Avl_Prev(&e->link) == 0);

    AvlU64Tree_Remove(&tt, 42);
    AssertEq(tt.tree.count, 0);
    AssertTrue(tt.tree.root == 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64Count)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AssertEq(tt.tree.count, 0);

    for (U64 i = 0; i < 10; i++)
    {
        AvlU64Tree_Insert(&tt, i, 0);
        AssertEq(tt.tree.count, i + 1);
    }

    AvlU64Tree_Insert(&tt, 5, 999);
    AssertEq(tt.tree.count, 10);

    for (U64 i = 0; i < 10; i++)
    {
        AvlU64Tree_Remove(&tt, i);
        AssertEq(tt.tree.count, 9 - i);
    }

    Scratch_End(scratch);
}

Test(Test_AvlU64ForEach)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    U64 vals[] = {50, 30, 70, 10, 40, 60, 80, 20};
    for (U64 i = 0; i < ArrayCount(vals); i++)
    {
        AvlU64Tree_Insert(&tt, vals[i], vals[i] * 10);
    }

    // Forward: ascending order
    U64 expected_fwd[] = {10, 20, 30, 40, 50, 60, 70, 80};
    U64 idx = 0;
    Avl_ForEach(AvlU64Node, e, &tt.tree, link)
    {
        AssertTrue(idx < ArrayCount(expected_fwd));
        AssertEq(e->key, expected_fwd[idx]);
        AssertEq(e->value, expected_fwd[idx] * 10);
        idx++;
    }
    AssertEq(idx, ArrayCount(expected_fwd));

    // Reverse: descending order
    U64 expected_rev[] = {80, 70, 60, 50, 40, 30, 20, 10};
    idx = 0;
    Avl_ForEachReverse(AvlU64Node, e, &tt.tree, link)
    {
        AssertTrue(idx < ArrayCount(expected_rev));
        AssertEq(e->key, expected_rev[idx]);
        idx++;
    }
    AssertEq(idx, ArrayCount(expected_rev));

    // Empty tree
    AvlU64Tree empty = {0};
    U64 empty_count = 0;
    Avl_ForEach(AvlU64Node, e, &empty.tree, link)
    {
        (void)e;
        empty_count++;
    }
    AssertEq(empty_count, 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64FindDirect)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 10, 100);
    AvlU64Tree_Insert(&tt, 20, 200);
    AvlU64Tree_Insert(&tt, 30, 300);

    // Found cases: Avl_Find returns the matching AvlLink.
    U64 key = 20;
    AvlLink *found = Avl_Find(&tt.tree, AvlU64_Cmp, &key);
    AssertTrue(found != 0);
    AvlU64Node *node = ContainerOf(found, AvlU64Node, link);
    AssertEq(node->key, 20);
    AssertEq(node->value, 200);

    // Not-found case: returns 0.
    U64 missing = 99;
    AssertTrue(Avl_Find(&tt.tree, AvlU64_Cmp, &missing) == 0);

    // Empty tree: returns 0.
    AvlTree empty = {0};
    AssertTrue(Avl_Find(&empty, AvlU64_Cmp, &key) == 0);

    Scratch_End(scratch);
}

Test(Test_AvlU64FindAfterRemove)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlU64Tree tt = AvlU64Tree_Create(a);

    AvlU64Tree_Insert(&tt, 10, 100);
    AvlU64Tree_Insert(&tt, 20, 200);
    AvlU64Tree_Insert(&tt, 30, 300);

    U64 key = 20;
    AssertTrue(Avl_Find(&tt.tree, AvlU64_Cmp, &key) != 0);

    AvlU64Tree_Remove(&tt, 20);
    AssertTrue(Avl_Find(&tt.tree, AvlU64_Cmp, &key) == 0);

    // Neighbours still findable.
    U64 k10 = 10;
    U64 k30 = 30;
    AssertTrue(Avl_Find(&tt.tree, AvlU64_Cmp, &k10) != 0);
    AssertTrue(Avl_Find(&tt.tree, AvlU64_Cmp, &k30) != 0);

    Scratch_End(scratch);
}

TestSuite suite_avl_u64 = TestSuite_Make(
    "AVL (u64)", TestCase_Make(Test_AvlU64InsertFind), TestCase_Make(Test_AvlU64InsertOverwrite),
    TestCase_Make(Test_AvlU64Remove), TestCase_Make(Test_AvlU64RemoveNonexistent), TestCase_Make(Test_AvlU64Ordering),
    TestCase_Make(Test_AvlU64ReverseOrdering), TestCase_Make(Test_AvlU64BalanceAscending),
    TestCase_Make(Test_AvlU64BalanceDescending), TestCase_Make(Test_AvlU64Large), TestCase_Make(Test_AvlU64Empty),
    TestCase_Make(Test_AvlU64Single), TestCase_Make(Test_AvlU64Count), TestCase_Make(Test_AvlU64ForEach),
    TestCase_Make(Test_AvlU64FindDirect), TestCase_Make(Test_AvlU64FindAfterRemove), );

// ===========================================================================
// Suite 2: String8-keyed with free list
// ===========================================================================

typedef struct AvlStr8Node AvlStr8Node;
struct AvlStr8Node
{
    AvlLink link;
    AvlStr8Node *free_next;
    String8 key;
    U64 value;
};

typedef struct AvlStr8Tree AvlStr8Tree;
struct AvlStr8Tree
{
    AvlTree tree; // AvlStr8Node
    Arena *arena;
    AvlStr8Node *free_list;
};

static AvlStr8Tree AvlStr8Tree_Create(Arena *a)
{
    AvlStr8Tree tt = {0};
    tt.arena = a;
    return tt;
}

static I32 AvlStr8_Cmp(void *link, void *ctx)
{
    AvlStr8Node *e = ContainerOf(link, AvlStr8Node, link);
    String8 *key = (String8 *)ctx;
    // Compare by hash first, then lexicographic as tiebreaker
    U64 ha = Hash_Fnv1a(e->key.data, e->key.len);
    U64 hb = Hash_Fnv1a(key->data, key->len);
    if (ha < hb)
        return -1;
    if (ha > hb)
        return 1;
    U64 n = Min(e->key.len, key->len);
    for (U64 i = 0; i < n; i++)
    {
        if (e->key.data[i] < key->data[i])
            return -1;
        if (e->key.data[i] > key->data[i])
            return 1;
    }
    if (e->key.len < key->len)
        return -1;
    if (e->key.len > key->len)
        return 1;
    return 0;
}

static AvlStr8Node *AvlStr8Tree_Find(AvlStr8Tree *tt, String8 key)
{
    AvlLink *found = Avl_Find(&tt->tree, AvlStr8_Cmp, &key);
    if (found == 0)
    {
        return 0;
    }
    return ContainerOf(found, AvlStr8Node, link);
}

static AvlStr8Node *AvlStr8Tree_Insert(AvlStr8Tree *tt, String8 key, U64 value)
{
    AvlLink **slot;
    AvlLink *parent = Avl_FindInsertLocation(&tt->tree, AvlStr8_Cmp, &key, &slot);

    if (*slot != 0)
    {
        AvlStr8Node *e = ContainerOf(*slot, AvlStr8Node, link);
        e->value = value;
        return e;
    }

    AvlStr8Node *node;
    if (tt->free_list != 0)
    {
        node = tt->free_list;
        tt->free_list = node->free_next;
        memset(node, 0, sizeof(*node));
    }
    else
    {
        node = Arena_PushStruct(tt->arena, AvlStr8Node);
    }

    node->key = String8_Copy(tt->arena, key);
    node->value = value;
    Avl_Insert(&tt->tree, parent, slot, &node->link);
    return node;
}

static void AvlStr8Tree_Remove(AvlStr8Tree *tt, String8 key)
{
    AvlStr8Node *e = AvlStr8Tree_Find(tt, key);
    if (e == 0)
    {
        return;
    }
    Avl_Remove(&tt->tree, &e->link);
    e->free_next = tt->free_list;
    tt->free_list = e;
}

// ---------------------------------------------------------------------------
// Suite 2 tests
// ---------------------------------------------------------------------------

Test(Test_AvlStr8InsertFind)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    AvlStr8Tree_Insert(&tt, String8_Lit("hello"), 1);
    AvlStr8Tree_Insert(&tt, String8_Lit("world"), 2);
    AvlStr8Tree_Insert(&tt, String8_Lit("foo"), 3);

    AvlStr8Node *e1 = AvlStr8Tree_Find(&tt, String8_Lit("hello"));
    AvlStr8Node *e2 = AvlStr8Tree_Find(&tt, String8_Lit("world"));
    AvlStr8Node *e3 = AvlStr8Tree_Find(&tt, String8_Lit("foo"));
    AvlStr8Node *e4 = AvlStr8Tree_Find(&tt, String8_Lit("missing"));

    AssertTrue(e1 != 0);
    AssertEq(e1->value, 1);
    AssertTrue(e2 != 0);
    AssertEq(e2->value, 2);
    AssertTrue(e3 != 0);
    AssertEq(e3->value, 3);
    AssertTrue(e4 == 0);

    Scratch_End(scratch);
}

Test(Test_AvlStr8InsertOverwrite)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    AvlStr8Tree_Insert(&tt, String8_Lit("key"), 100);
    AssertEq(tt.tree.count, 1);

    AvlStr8Tree_Insert(&tt, String8_Lit("key"), 200);
    AssertEq(tt.tree.count, 1);

    AvlStr8Node *e = AvlStr8Tree_Find(&tt, String8_Lit("key"));
    AssertTrue(e != 0);
    AssertEq(e->value, 200);

    Scratch_End(scratch);
}

Test(Test_AvlStr8Remove)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    AvlStr8Tree_Insert(&tt, String8_Lit("a"), 1);
    AvlStr8Tree_Insert(&tt, String8_Lit("b"), 2);
    AvlStr8Tree_Insert(&tt, String8_Lit("c"), 3);

    AvlStr8Tree_Remove(&tt, String8_Lit("b"));

    AssertTrue(AvlStr8Tree_Find(&tt, String8_Lit("a")) != 0);
    AssertTrue(AvlStr8Tree_Find(&tt, String8_Lit("b")) == 0);
    AssertTrue(AvlStr8Tree_Find(&tt, String8_Lit("c")) != 0);

    Scratch_End(scratch);
}

Test(Test_AvlStr8FreeListReuse)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    AvlStr8Tree_Insert(&tt, String8_Lit("temp"), 100);
    AvlStr8Tree_Remove(&tt, String8_Lit("temp"));

    AssertTrue(tt.free_list != 0);

    AvlStr8Tree_Insert(&tt, String8_Lit("re"), 200);

    // String8_Copy allocates, but the node itself came from the free list.
    AvlStr8Node *e = AvlStr8Tree_Find(&tt, String8_Lit("re"));
    AssertTrue(e != 0);
    AssertEq(e->value, 200);

    // Verify the free list was consumed
    AssertTrue(tt.free_list == 0);

    Scratch_End(scratch);
}

Test(Test_AvlStr8ManyKeys)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    for (U64 i = 0; i < 100; i++)
    {
        String8 key = String8_PushF(a, "key_%llu", (unsigned long long)i);
        AvlStr8Tree_Insert(&tt, key, i);
    }
    AssertEq(tt.tree.count, 100);

    for (U64 i = 0; i < 100; i++)
    {
        String8 key = String8_PushF(a, "key_%llu", (unsigned long long)i);
        AvlStr8Node *e = AvlStr8Tree_Find(&tt, key);
        AssertTrue(e != 0);
        AssertEq(e->value, i);
    }

    AvlTest_AssertBalanced(tt.tree.root);

    // Remove odd-numbered keys
    for (U64 i = 1; i < 100; i += 2)
    {
        String8 key = String8_PushF(a, "key_%llu", (unsigned long long)i);
        AvlStr8Tree_Remove(&tt, key);
    }
    AssertEq(tt.tree.count, 50);

    for (U64 i = 0; i < 100; i++)
    {
        String8 key = String8_PushF(a, "key_%llu", (unsigned long long)i);
        AvlStr8Node *e = AvlStr8Tree_Find(&tt, key);
        if (i % 2 == 0)
        {
            AssertTrue(e != 0);
        }
        else
        {
            AssertTrue(e == 0);
        }
    }

    AvlTest_AssertBalanced(tt.tree.root);

    Scratch_End(scratch);
}

Test(Test_AvlStr8Ordering)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AvlStr8Tree tt = AvlStr8Tree_Create(a);

    AvlStr8Tree_Insert(&tt, String8_Lit("cherry"), 1);
    AvlStr8Tree_Insert(&tt, String8_Lit("apple"), 2);
    AvlStr8Tree_Insert(&tt, String8_Lit("banana"), 3);
    AvlStr8Tree_Insert(&tt, String8_Lit("date"), 4);

    // Verify in-order traversal works (order is by hash, not lexicographic)
    U64 count = 0;
    Avl_ForEach(AvlStr8Node, e, &tt.tree, link)
    {
        (void)e;
        count++;
    }
    AssertEq(count, 4);

    Scratch_End(scratch);
}

TestSuite suite_avl_str8 =
    TestSuite_Make("AVL (String8)", TestCase_Make(Test_AvlStr8InsertFind), TestCase_Make(Test_AvlStr8InsertOverwrite),
                   TestCase_Make(Test_AvlStr8Remove), TestCase_Make(Test_AvlStr8FreeListReuse),
                   TestCase_Make(Test_AvlStr8ManyKeys), TestCase_Make(Test_AvlStr8Ordering), );
