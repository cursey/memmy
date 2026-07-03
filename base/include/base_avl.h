#ifndef BASE_AVL_H
#define BASE_AVL_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// AVL Tree (intrusive)
// ---------------------------------------------------------------------------

typedef struct AvlLink AvlLink;
struct AvlLink
{
    AvlLink *left;
    AvlLink *right;
    AvlLink *parent;
    I32 height;
};

typedef struct AvlTree AvlTree;
struct AvlTree
{
    AvlLink *root;
    U64 count;
};

typedef I32 (*AvlCmpFn)(void *link, void *ctx);

AvlLink *Avl_Find(AvlTree *tree, AvlCmpFn cmp, void *ctx);
AvlLink *Avl_FindInsertLocation(AvlTree *tree, AvlCmpFn cmp, void *ctx, AvlLink ***out_slot);
void Avl_Insert(AvlTree *tree, AvlLink *parent, AvlLink **slot, AvlLink *link);
void Avl_Remove(AvlTree *tree, AvlLink *link);
AvlLink *Avl_Min(AvlTree *tree);
AvlLink *Avl_Max(AvlTree *tree);
AvlLink *Avl_Next(AvlLink *link);
AvlLink *Avl_Prev(AvlLink *link);

#define Avl_ForEach(T, var, tree, member)                                                                              \
    for (T *var = (Avl_Min(tree) != 0 ? ContainerOf(Avl_Min(tree), T, member) : (T *)0); var != 0;                     \
         var = (Avl_Next(&var->member) != 0 ? ContainerOf(Avl_Next(&var->member), T, member) : (T *)0))

#define Avl_ForEachReverse(T, var, tree, member)                                                                       \
    for (T *var = (Avl_Max(tree) != 0 ? ContainerOf(Avl_Max(tree), T, member) : (T *)0); var != 0;                     \
         var = (Avl_Prev(&var->member) != 0 ? ContainerOf(Avl_Prev(&var->member), T, member) : (T *)0))

#endif // BASE_AVL_H
