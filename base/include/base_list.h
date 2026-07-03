#ifndef BASE_LIST_H
#define BASE_LIST_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Intrusive doubly-linked list (null-terminated, zero-initializable)
// ---------------------------------------------------------------------------

typedef struct ListLink ListLink;
struct ListLink
{
    ListLink *next;
    ListLink *prev;
};

typedef struct List List;
struct List
{
    ListLink *first;
    ListLink *last;
    U64 count;
};

B32 List_IsEmpty(List *list);
void List_PushBack(List *list, ListLink *link);
void List_PushFront(List *list, ListLink *link);
ListLink *List_PopFront(List *list);
ListLink *List_PopBack(List *list);
void List_InsertBefore(List *list, ListLink *anchor, ListLink *link);
void List_InsertAfter(List *list, ListLink *anchor, ListLink *link);
void List_Remove(List *list, ListLink *link);

#define List_ForEach(T, var, list, member)                                                                             \
    for (T *var = ((list)->first != 0 ? ContainerOf((list)->first, T, member) : (T *)0); var != 0;                     \
         var = (var->member.next != 0 ? ContainerOf(var->member.next, T, member) : (T *)0))

#define List_ForEachReverse(T, var, list, member)                                                                      \
    for (T *var = ((list)->last != 0 ? ContainerOf((list)->last, T, member) : (T *)0); var != 0;                       \
         var = (var->member.prev != 0 ? ContainerOf(var->member.prev, T, member) : (T *)0))

#endif // BASE_LIST_H
