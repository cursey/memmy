// ===========================================================================
// Intrusive hash map tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

// ---------------------------------------------------------------------------
// Test node types
// ---------------------------------------------------------------------------

typedef struct U64Entry U64Entry;
struct U64Entry
{
    HashLink hash_link;
    U64 key;
    U64 value;
};

typedef struct Str8Entry Str8Entry;
struct Str8Entry
{
    HashLink hash_link;
    String8 key;
    U64 value;
};

// ---------------------------------------------------------------------------
// Equality callbacks
// ---------------------------------------------------------------------------

static B32 U64Entry_Eq(void *link, void *ctx)
{
    U64Entry *entry = ContainerOf(link, U64Entry, hash_link);
    U64 *key = (U64 *)ctx;
    return entry->key == *key;
}

static B32 Str8Entry_Eq(void *link, void *ctx)
{
    Str8Entry *entry = ContainerOf(link, Str8Entry, hash_link);
    String8 *key = (String8 *)ctx;
    return String8_Eq(entry->key, *key);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static U64Entry *U64Map_Put(Arena *arena, HashMap *map, U64 key, U64 value)
{
    U64 hash = Hash_U64(key);
    HashLink *existing = HashMap_Find(map, hash, U64Entry_Eq, &key);
    if (existing != 0)
    {
        U64Entry *entry = ContainerOf(existing, U64Entry, hash_link);
        entry->value = value;
        return entry;
    }
    U64Entry *entry = Arena_PushStruct(arena, U64Entry);
    entry->key = key;
    entry->value = value;
    entry->hash_link.hash = hash;
    HashMap_Insert(map, &entry->hash_link);
    return entry;
}

static U64Entry *U64Map_Get(HashMap *map, U64 key)
{
    U64 hash = Hash_U64(key);
    HashLink *link = HashMap_Find(map, hash, U64Entry_Eq, &key);
    if (link == 0)
    {
        return 0;
    }
    return ContainerOf(link, U64Entry, hash_link);
}

static Str8Entry *Str8Map_Put(Arena *arena, HashMap *map, String8 key, U64 value)
{
    U64 hash = Hash_Fnv1a(key.data, key.len);
    HashLink *existing = HashMap_Find(map, hash, Str8Entry_Eq, &key);
    if (existing != 0)
    {
        Str8Entry *entry = ContainerOf(existing, Str8Entry, hash_link);
        entry->value = value;
        return entry;
    }
    Str8Entry *entry = Arena_PushStruct(arena, Str8Entry);
    entry->key = key;
    entry->value = value;
    entry->hash_link.hash = hash;
    HashMap_Insert(map, &entry->hash_link);
    return entry;
}

static Str8Entry *Str8Map_Get(HashMap *map, String8 key)
{
    U64 hash = Hash_Fnv1a(key.data, key.len);
    HashLink *link = HashMap_Find(map, hash, Str8Entry_Eq, &key);
    if (link == 0)
    {
        return 0;
    }
    return ContainerOf(link, Str8Entry, hash_link);
}

// ---------------------------------------------------------------------------
// Tests — U64-keyed
// ---------------------------------------------------------------------------

Test(Test_HashMapU64PutGet)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    U64Map_Put(a, &map, 10, 100);
    U64Map_Put(a, &map, 20, 200);

    U64Entry *n1 = U64Map_Get(&map, 10);
    U64Entry *n2 = U64Map_Get(&map, 20);
    U64Entry *n3 = U64Map_Get(&map, 30);

    AssertTrue(n1 != 0);
    AssertEq(n1->value, 100);
    AssertTrue(n2 != 0);
    AssertEq(n2->value, 200);
    AssertTrue(n3 == 0);

    Scratch_End(scratch);
}

Test(Test_HashMapU64Overwrite)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    U64Map_Put(a, &map, 42, 1);
    U64Map_Put(a, &map, 42, 2);

    U64Entry *n = U64Map_Get(&map, 42);
    AssertTrue(n != 0);
    AssertEq(n->value, 2);
    AssertEq(map.count, 1);

    Scratch_End(scratch);
}

Test(Test_HashMapU64Remove)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    U64Map_Put(a, &map, 1, 10);
    U64Map_Put(a, &map, 2, 20);
    U64Map_Put(a, &map, 3, 30);

    U64Entry *n = U64Map_Get(&map, 2);
    AssertTrue(n != 0);
    HashMap_Remove(&map, &n->hash_link);

    AssertTrue(U64Map_Get(&map, 1) != 0);
    AssertTrue(U64Map_Get(&map, 2) == 0);
    AssertTrue(U64Map_Get(&map, 3) != 0);
    AssertEq(map.count, 2);

    Scratch_End(scratch);
}

Test(Test_HashMapU64ManyEntries)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    for (U64 i = 0; i < 1000; i++)
    {
        U64Map_Put(a, &map, i, i * 10);
    }

    AssertEq(map.count, 1000);

    for (U64 i = 0; i < 1000; i++)
    {
        U64Entry *n = U64Map_Get(&map, i);
        AssertTrue(n != 0);
        AssertEq(n->value, i * 10);
    }

    Scratch_End(scratch);
}

Test(Test_HashMapU64GrowthNoWaste)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    // Insert enough entries to trigger multiple growth steps
    for (U64 i = 0; i < 200; i++)
    {
        U64Map_Put(a, &map, i, i);
    }

    // All entries must still be findable after growth
    for (U64 i = 0; i < 200; i++)
    {
        U64Entry *n = U64Map_Get(&map, i);
        AssertTrue(n != 0);
        AssertEq(n->value, i);
    }

    // Bucket count should be >= count (load factor)
    AssertTrue(map.bucket_count >= map.count);

    Scratch_End(scratch);
}

// ---------------------------------------------------------------------------
// Tests — String8-keyed
// ---------------------------------------------------------------------------

Test(Test_HashMapStr8PutGet)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    Str8Map_Put(a, &map, String8_Lit("hello"), 42);
    Str8Map_Put(a, &map, String8_Lit("world"), 99);

    Str8Entry *n1 = Str8Map_Get(&map, String8_Lit("hello"));
    Str8Entry *n2 = Str8Map_Get(&map, String8_Lit("world"));
    Str8Entry *n3 = Str8Map_Get(&map, String8_Lit("missing"));

    AssertTrue(n1 != 0);
    AssertEq(n1->value, 42);
    AssertTrue(n2 != 0);
    AssertEq(n2->value, 99);
    AssertTrue(n3 == 0);

    Scratch_End(scratch);
}

Test(Test_HashMapStr8Overwrite)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    Str8Map_Put(a, &map, String8_Lit("key"), 1);
    Str8Map_Put(a, &map, String8_Lit("key"), 2);

    Str8Entry *n = Str8Map_Get(&map, String8_Lit("key"));
    AssertTrue(n != 0);
    AssertEq(n->value, 2);

    Scratch_End(scratch);
}

Test(Test_HashMapStr8Remove)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    Str8Map_Put(a, &map, String8_Lit("a"), 1);
    Str8Map_Put(a, &map, String8_Lit("b"), 2);
    Str8Map_Put(a, &map, String8_Lit("c"), 3);

    Str8Entry *n = Str8Map_Get(&map, String8_Lit("b"));
    AssertTrue(n != 0);
    HashMap_Remove(&map, &n->hash_link);

    AssertTrue(Str8Map_Get(&map, String8_Lit("a")) != 0);
    AssertTrue(Str8Map_Get(&map, String8_Lit("b")) == 0);
    AssertTrue(Str8Map_Get(&map, String8_Lit("c")) != 0);

    Scratch_End(scratch);
}

// ---------------------------------------------------------------------------
// Tests — Iteration
// ---------------------------------------------------------------------------

Test(Test_HashMapForEach)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    HashMap map = HashMap_Create(a);

    U64Map_Put(a, &map, 1, 10);
    U64Map_Put(a, &map, 2, 20);
    U64Map_Put(a, &map, 3, 30);

    U64 sum = 0;
    U64 visited = 0;
    HashMap_ForEach(U64Entry, entry, &map, hash_link)
    {
        sum += entry->value;
        visited++;
    }

    AssertEq(visited, 3);
    AssertEq(sum, 60);

    Scratch_End(scratch);
}

Test(Test_HashMapForEachEmpty)
{
    Scratch scratch = Scratch_Begin(0, 0);
    HashMap map = HashMap_Create(scratch.arena);

    U64 visited = 0;
    HashMap_ForEach(U64Entry, entry, &map, hash_link)
    {
        visited++;
    }

    AssertEq(visited, 0);

    Scratch_End(scratch);
}

// ---------------------------------------------------------------------------
// Suites
// ---------------------------------------------------------------------------

TestSuite suite_hashmap =
    TestSuite_Make("HashMap", TestCase_Make(Test_HashMapU64PutGet), TestCase_Make(Test_HashMapU64Overwrite),
                   TestCase_Make(Test_HashMapU64Remove), TestCase_Make(Test_HashMapU64ManyEntries),
                   TestCase_Make(Test_HashMapU64GrowthNoWaste), TestCase_Make(Test_HashMapStr8PutGet),
                   TestCase_Make(Test_HashMapStr8Overwrite), TestCase_Make(Test_HashMapStr8Remove),
                   TestCase_Make(Test_HashMapForEach), TestCase_Make(Test_HashMapForEachEmpty), );
