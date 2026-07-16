// ===========================================================================
// List (doubly-linked, null-terminated) tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

typedef struct TestListNode TestListNode;
struct TestListNode
{
    ListLink link;
    U64 val;
};

Test(Test_ListIsEmpty)
{
    List list = {0};
    AssertTrue(List_IsEmpty(&list));

    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;

    List_PushBack(&list, &n1->link);
    AssertTrue(!List_IsEmpty(&list));

    List_Remove(&list, &n1->link);
    AssertTrue(List_IsEmpty(&list));

    Scratch_End(scratch);
}

Test(Test_ListPushBack)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    // Forward: 1, 2, 3
    AssertTrue(list.first == &n1->link);
    AssertTrue(list.last == &n3->link);
    AssertEq(ContainerOf(list.first, TestListNode, link)->val, 1);
    AssertEq(ContainerOf(list.first->next, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.first->next->next, TestListNode, link)->val, 3);

    // Backward: 3, 2, 1
    AssertEq(ContainerOf(list.last, TestListNode, link)->val, 3);
    AssertEq(ContainerOf(list.last->prev, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.last->prev->prev, TestListNode, link)->val, 1);

    // Null-terminated
    AssertTrue(list.first->next->next->next == 0);
    AssertTrue(list.last->prev->prev->prev == 0);

    Scratch_End(scratch);
}

Test(Test_ListPushFront)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushFront(&list, &n1->link);
    List_PushFront(&list, &n2->link);
    List_PushFront(&list, &n3->link);

    // Forward: 3, 2, 1
    AssertTrue(list.first == &n3->link);
    AssertTrue(list.last == &n1->link);
    AssertEq(ContainerOf(list.first, TestListNode, link)->val, 3);
    AssertEq(ContainerOf(list.first->next, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.first->next->next, TestListNode, link)->val, 1);

    // Backward: 1, 2, 3
    AssertEq(ContainerOf(list.last, TestListNode, link)->val, 1);
    AssertEq(ContainerOf(list.last->prev, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.last->prev->prev, TestListNode, link)->val, 3);

    // Null-terminated
    AssertTrue(list.first->next->next->next == 0);
    AssertTrue(list.last->prev->prev->prev == 0);

    Scratch_End(scratch);
}

Test(Test_ListRemoveMiddle)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    List_Remove(&list, &n2->link);

    AssertTrue(list.first == &n1->link);
    AssertTrue(list.last == &n3->link);
    AssertTrue(n1->link.next == &n3->link);
    AssertTrue(n3->link.prev == &n1->link);
    AssertTrue(n2->link.next == 0);
    AssertTrue(n2->link.prev == 0);

    Scratch_End(scratch);
}

Test(Test_ListRemoveFirst)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);

    List_Remove(&list, &n1->link);

    AssertTrue(list.first == &n2->link);
    AssertTrue(list.last == &n2->link);
    AssertTrue(n2->link.prev == 0);
    AssertTrue(n2->link.next == 0);

    Scratch_End(scratch);
}

Test(Test_ListRemoveLast)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);

    List_Remove(&list, &n2->link);

    AssertTrue(list.first == &n1->link);
    AssertTrue(list.last == &n1->link);
    AssertTrue(n1->link.prev == 0);
    AssertTrue(n1->link.next == 0);

    Scratch_End(scratch);
}

Test(Test_ListRemoveOnly)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;

    List_PushBack(&list, &n1->link);
    List_Remove(&list, &n1->link);

    AssertTrue(List_IsEmpty(&list));

    Scratch_End(scratch);
}

typedef struct TestMultiNode TestMultiNode;
struct TestMultiNode
{
    ListLink hash_link;
    ListLink lru_link;
    U64 val;
};

Test(Test_ListMultiList)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List hash_list = {0};
    List lru_list = {0};

    TestMultiNode *n1 = Arena_PushStruct(a, TestMultiNode);
    TestMultiNode *n2 = Arena_PushStruct(a, TestMultiNode);
    n1->val = 1;
    n2->val = 2;

    // Add to both lists
    List_PushBack(&hash_list, &n1->hash_link);
    List_PushBack(&hash_list, &n2->hash_link);
    List_PushBack(&lru_list, &n2->lru_link);
    List_PushBack(&lru_list, &n1->lru_link);

    // Hash order: 1, 2
    AssertEq(ContainerOf(hash_list.first, TestMultiNode, hash_link)->val, 1);
    AssertEq(ContainerOf(hash_list.last, TestMultiNode, hash_link)->val, 2);

    // LRU order: 2, 1
    AssertEq(ContainerOf(lru_list.first, TestMultiNode, lru_link)->val, 2);
    AssertEq(ContainerOf(lru_list.last, TestMultiNode, lru_link)->val, 1);

    // Remove n1 from hash only
    List_Remove(&hash_list, &n1->hash_link);
    AssertTrue(hash_list.first == &n2->hash_link);
    AssertTrue(hash_list.last == &n2->hash_link);

    // n1 still in LRU
    AssertEq(ContainerOf(lru_list.last, TestMultiNode, lru_link)->val, 1);

    Scratch_End(scratch);
}

Test(Test_ListCount)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    AssertEq(list.count, 0);

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushBack(&list, &n1->link);
    AssertEq(list.count, 1);
    List_PushBack(&list, &n2->link);
    AssertEq(list.count, 2);
    List_PushFront(&list, &n3->link);
    AssertEq(list.count, 3);

    List_Remove(&list, &n2->link);
    AssertEq(list.count, 2);
    List_Remove(&list, &n1->link);
    AssertEq(list.count, 1);
    List_Remove(&list, &n3->link);
    AssertEq(list.count, 0);

    Scratch_End(scratch);
}

Test(Test_ListForEach)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 10;
    n2->val = 20;
    n3->val = 30;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    U64 sum = 0;
    U64 count = 0;
    List_ForEach(TestListNode, n, &list, link)
    {
        sum += n->val;
        count++;
    }
    AssertEq(sum, 60);
    AssertEq(count, 3);

    // Empty list
    List empty = {0};
    U64 empty_count = 0;
    List_ForEach(TestListNode, n, &empty, link)
    {
        (void)n;
        empty_count++;
    }
    AssertEq(empty_count, 0);

    Scratch_End(scratch);
}

Test(Test_ListForEachBreak)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    for (U64 i = 1; i <= 5; i++)
    {
        TestListNode *n = Arena_PushStruct(a, TestListNode);
        n->val = i;
        List_PushBack(&list, &n->link);
    }

    // Break after finding val == 3
    U64 found = 0;
    List_ForEach(TestListNode, n, &list, link)
    {
        if (n->val == 3)
        {
            found = n->val;
            break;
        }
    }
    AssertEq(found, 3);

    Scratch_End(scratch);
}

Test(Test_ListPopFront)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    ListLink *e1 = List_PopFront(&list);
    AssertTrue(e1 == &n1->link);
    AssertTrue(e1->next == 0);
    AssertEq(list.count, 2);

    ListLink *e2 = List_PopFront(&list);
    AssertTrue(e2 == &n2->link);
    AssertEq(list.count, 1);

    ListLink *e3 = List_PopFront(&list);
    AssertTrue(e3 == &n3->link);
    AssertEq(list.count, 0);
    AssertTrue(List_IsEmpty(&list));
    AssertTrue(list.last == 0);

    Scratch_End(scratch);
}

Test(Test_ListPopBack)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    ListLink *e3 = List_PopBack(&list);
    AssertTrue(e3 == &n3->link);
    AssertTrue(e3->prev == 0);
    AssertEq(list.count, 2);

    ListLink *e2 = List_PopBack(&list);
    AssertTrue(e2 == &n2->link);
    AssertEq(list.count, 1);

    ListLink *e1 = List_PopBack(&list);
    AssertTrue(e1 == &n1->link);
    AssertEq(list.count, 0);
    AssertTrue(List_IsEmpty(&list));
    AssertTrue(list.first == 0);

    Scratch_End(scratch);
}

Test(Test_ListForEachReverse)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 10;
    n2->val = 20;
    n3->val = 30;

    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n2->link);
    List_PushBack(&list, &n3->link);

    U64 sum = 0;
    U64 count = 0;
    U64 prev = 40;
    List_ForEachReverse(TestListNode, n, &list, link)
    {
        AssertTrue(n->val < prev);
        prev = n->val;
        sum += n->val;
        count++;
    }
    AssertEq(sum, 60);
    AssertEq(count, 3);

    // Empty list
    List empty = {0};
    U64 empty_count = 0;
    List_ForEachReverse(TestListNode, n, &empty, link)
    {
        (void)n;
        empty_count++;
    }
    AssertEq(empty_count, 0);

    Scratch_End(scratch);
}

Test(Test_ListInsertBefore)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    // Build list: [1, 3]
    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n3->link);

    // Insert 2 before 3 => [1, 2, 3]
    List_InsertBefore(&list, &n3->link, &n2->link);

    AssertEq(list.count, 3);
    AssertEq(ContainerOf(list.first, TestListNode, link)->val, 1);
    AssertEq(ContainerOf(list.first->next, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.first->next->next, TestListNode, link)->val, 3);
    AssertTrue(list.first->next->next->next == 0);

    // Backward: 3, 2, 1
    AssertEq(ContainerOf(list.last, TestListNode, link)->val, 3);
    AssertEq(ContainerOf(list.last->prev, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.last->prev->prev, TestListNode, link)->val, 1);
    AssertTrue(list.last->prev->prev->prev == 0);

    Scratch_End(scratch);
}

Test(Test_ListInsertBeforeFirst)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;

    List_PushBack(&list, &n2->link);

    // Insert 1 before first => [1, 2]
    List_InsertBefore(&list, &n2->link, &n1->link);

    AssertEq(list.count, 2);
    AssertTrue(list.first == &n1->link);
    AssertTrue(list.last == &n2->link);
    AssertTrue(n1->link.prev == 0);
    AssertTrue(n1->link.next == &n2->link);
    AssertTrue(n2->link.prev == &n1->link);
    AssertTrue(n2->link.next == 0);

    Scratch_End(scratch);
}

Test(Test_ListInsertAfter)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    TestListNode *n3 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;
    n3->val = 3;

    // Build list: [1, 3]
    List_PushBack(&list, &n1->link);
    List_PushBack(&list, &n3->link);

    // Insert 2 after 1 => [1, 2, 3]
    List_InsertAfter(&list, &n1->link, &n2->link);

    AssertEq(list.count, 3);
    AssertEq(ContainerOf(list.first, TestListNode, link)->val, 1);
    AssertEq(ContainerOf(list.first->next, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.first->next->next, TestListNode, link)->val, 3);
    AssertTrue(list.first->next->next->next == 0);

    // Backward: 3, 2, 1
    AssertEq(ContainerOf(list.last, TestListNode, link)->val, 3);
    AssertEq(ContainerOf(list.last->prev, TestListNode, link)->val, 2);
    AssertEq(ContainerOf(list.last->prev->prev, TestListNode, link)->val, 1);
    AssertTrue(list.last->prev->prev->prev == 0);

    Scratch_End(scratch);
}

Test(Test_ListInsertAfterLast)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    List list = {0};

    TestListNode *n1 = Arena_PushStruct(a, TestListNode);
    TestListNode *n2 = Arena_PushStruct(a, TestListNode);
    n1->val = 1;
    n2->val = 2;

    List_PushBack(&list, &n1->link);

    // Insert 2 after last => [1, 2]
    List_InsertAfter(&list, &n1->link, &n2->link);

    AssertEq(list.count, 2);
    AssertTrue(list.first == &n1->link);
    AssertTrue(list.last == &n2->link);
    AssertTrue(n1->link.prev == 0);
    AssertTrue(n1->link.next == &n2->link);
    AssertTrue(n2->link.prev == &n1->link);
    AssertTrue(n2->link.next == 0);

    Scratch_End(scratch);
}

TestSuite suite_list = TestSuite_Make(
    "List", TestCase_Make(Test_ListIsEmpty), TestCase_Make(Test_ListPushBack), TestCase_Make(Test_ListPushFront),
    TestCase_Make(Test_ListRemoveMiddle), TestCase_Make(Test_ListRemoveFirst), TestCase_Make(Test_ListRemoveLast),
    TestCase_Make(Test_ListRemoveOnly), TestCase_Make(Test_ListMultiList), TestCase_Make(Test_ListCount),
    TestCase_Make(Test_ListPopFront), TestCase_Make(Test_ListPopBack), TestCase_Make(Test_ListForEach),
    TestCase_Make(Test_ListForEachBreak), TestCase_Make(Test_ListForEachReverse), TestCase_Make(Test_ListInsertBefore),
    TestCase_Make(Test_ListInsertBeforeFirst), TestCase_Make(Test_ListInsertAfter),
    TestCase_Make(Test_ListInsertAfterLast), );
