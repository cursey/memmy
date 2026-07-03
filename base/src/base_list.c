#include "base_list.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// Intrusive doubly-linked list (null-terminated)
// ---------------------------------------------------------------------------

B32 List_IsEmpty(List *list)
{
    return list->first == 0;
}

void List_PushBack(List *list, ListLink *link)
{
    link->next = 0;
    link->prev = list->last;
    if (list->last != 0)
    {
        list->last->next = link;
    }
    else
    {
        list->first = link;
    }
    list->last = link;
    list->count++;
}

void List_PushFront(List *list, ListLink *link)
{
    link->prev = 0;
    link->next = list->first;
    if (list->first != 0)
    {
        list->first->prev = link;
    }
    else
    {
        list->last = link;
    }
    list->first = link;
    list->count++;
}

ListLink *List_PopFront(List *list)
{
    ListLink *link = list->first;
    list->first = link->next;
    if (list->first != 0)
    {
        list->first->prev = 0;
    }
    else
    {
        list->last = 0;
    }
    link->next = 0;
    list->count--;
    return link;
}

ListLink *List_PopBack(List *list)
{
    ListLink *link = list->last;
    list->last = link->prev;
    if (list->last != 0)
    {
        list->last->next = 0;
    }
    else
    {
        list->first = 0;
    }
    link->prev = 0;
    list->count--;
    return link;
}

void List_InsertBefore(List *list, ListLink *anchor, ListLink *link)
{
    link->prev = anchor->prev;
    link->next = anchor;
    if (anchor->prev != 0)
    {
        anchor->prev->next = link;
    }
    else
    {
        list->first = link;
    }
    anchor->prev = link;
    list->count++;
}

void List_InsertAfter(List *list, ListLink *anchor, ListLink *link)
{
    link->next = anchor->next;
    link->prev = anchor;
    if (anchor->next != 0)
    {
        anchor->next->prev = link;
    }
    else
    {
        list->last = link;
    }
    anchor->next = link;
    list->count++;
}

void List_Remove(List *list, ListLink *link)
{
    if (link->prev != 0)
    {
        link->prev->next = link->next;
    }
    else
    {
        list->first = link->next;
    }
    if (link->next != 0)
    {
        link->next->prev = link->prev;
    }
    else
    {
        list->last = link->prev;
    }
    link->next = 0;
    link->prev = 0;
    list->count--;
}
