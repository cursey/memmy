#include "base_avl.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// AVL Tree (intrusive)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static I32 Avl_Height(AvlLink *n)
{
    return (n != 0) ? n->height : -1;
}

static void Avl_UpdateHeight(AvlLink *n)
{
    I32 lh = Avl_Height(n->left);
    I32 rh = Avl_Height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

static I32 Avl_BalanceFactor(AvlLink *n)
{
    return Avl_Height(n->right) - Avl_Height(n->left);
}

static void Avl_ReplaceChild(AvlTree *tree, AvlLink *parent, AvlLink *old_child, AvlLink *new_child)
{
    if (parent == 0)
    {
        tree->root = new_child;
    }
    else if (parent->left == old_child)
    {
        parent->left = new_child;
    }
    else
    {
        parent->right = new_child;
    }
    if (new_child != 0)
    {
        new_child->parent = parent;
    }
}

// ---------------------------------------------------------------------------
// Rotations
// ---------------------------------------------------------------------------

static AvlLink *Avl_RotateLeft(AvlTree *tree, AvlLink *x)
{
    AvlLink *y = x->right;
    AvlLink *b = y->left;

    Avl_ReplaceChild(tree, x->parent, x, y);

    y->left = x;
    x->parent = y;

    x->right = b;
    if (b != 0)
    {
        b->parent = x;
    }

    Avl_UpdateHeight(x);
    Avl_UpdateHeight(y);
    return y;
}

static AvlLink *Avl_RotateRight(AvlTree *tree, AvlLink *x)
{
    AvlLink *y = x->left;
    AvlLink *b = y->right;

    Avl_ReplaceChild(tree, x->parent, x, y);

    y->right = x;
    x->parent = y;

    x->left = b;
    if (b != 0)
    {
        b->parent = x;
    }

    Avl_UpdateHeight(x);
    Avl_UpdateHeight(y);
    return y;
}

// ---------------------------------------------------------------------------
// Rebalance
// ---------------------------------------------------------------------------

static void Avl_Rebalance(AvlTree *tree, AvlLink *n)
{
    while (n != 0)
    {
        Avl_UpdateHeight(n);
        I32 bf = Avl_BalanceFactor(n);
        AvlLink *p = n->parent;

        if (bf < -1)
        {
            if (Avl_BalanceFactor(n->left) > 0)
            {
                Avl_RotateLeft(tree, n->left);
            }
            Avl_RotateRight(tree, n);
        }
        else if (bf > 1)
        {
            if (Avl_BalanceFactor(n->right) < 0)
            {
                Avl_RotateRight(tree, n->right);
            }
            Avl_RotateLeft(tree, n);
        }

        n = p;
    }
}

// ---------------------------------------------------------------------------
// Swap two entries' positions in the tree (for removal with two children)
// ---------------------------------------------------------------------------

static void Avl_SwapEntries(AvlTree *tree, AvlLink *a, AvlLink *b)
{
    AvlLink *a_parent = a->parent;
    AvlLink *a_left = a->left;
    AvlLink *a_right = a->right;
    I32 a_height = a->height;

    AvlLink *b_parent = b->parent;
    AvlLink *b_left = b->left;
    AvlLink *b_right = b->right;
    I32 b_height = b->height;

    // Handle the case where a is b's direct parent
    if (a == b_parent)
    {
        // a is parent of b
        Avl_ReplaceChild(tree, a_parent, a, b);
        b->parent = a_parent;

        if (a_left == b)
        {
            b->left = a;
            b->right = a_right;
            if (a_right != 0)
            {
                a_right->parent = b;
            }
        }
        else
        {
            b->right = a;
            b->left = a_left;
            if (a_left != 0)
            {
                a_left->parent = b;
            }
        }

        a->parent = b;
        a->left = b_left;
        a->right = b_right;
        if (b_left != 0)
        {
            b_left->parent = a;
        }
        if (b_right != 0)
        {
            b_right->parent = a;
        }
    }
    else if (b == a_parent)
    {
        // b is parent of a (mirror)
        Avl_ReplaceChild(tree, b_parent, b, a);
        a->parent = b_parent;

        if (b_left == a)
        {
            a->left = b;
            a->right = b_right;
            if (b_right != 0)
            {
                b_right->parent = a;
            }
        }
        else
        {
            a->right = b;
            a->left = b_left;
            if (b_left != 0)
            {
                b_left->parent = a;
            }
        }

        b->parent = a;
        b->left = a_left;
        b->right = a_right;
        if (a_left != 0)
        {
            a_left->parent = b;
        }
        if (a_right != 0)
        {
            a_right->parent = b;
        }
    }
    else
    {
        // General case: not adjacent
        Avl_ReplaceChild(tree, a_parent, a, b);
        Avl_ReplaceChild(tree, b_parent, b, a);

        a->left = b_left;
        a->right = b_right;
        if (b_left != 0)
        {
            b_left->parent = a;
        }
        if (b_right != 0)
        {
            b_right->parent = a;
        }

        b->left = a_left;
        b->right = a_right;
        if (a_left != 0)
        {
            a_left->parent = b;
        }
        if (a_right != 0)
        {
            a_right->parent = b;
        }
    }

    a->height = b_height;
    b->height = a_height;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

AvlLink *Avl_Find(AvlTree *tree, AvlCmpFn cmp, void *ctx)
{
    AvlLink *n = tree->root;
    while (n != 0)
    {
        I32 c = cmp(n, ctx);
        if (c < 0)
        {
            n = n->right;
        }
        else if (c > 0)
        {
            n = n->left;
        }
        else
        {
            return n;
        }
    }
    return 0;
}

AvlLink *Avl_FindInsertLocation(AvlTree *tree, AvlCmpFn cmp, void *ctx, AvlLink ***out_slot)
{
    AvlLink *parent = 0;
    AvlLink **slot = &tree->root;

    while (*slot != 0)
    {
        parent = *slot;
        I32 c = cmp(parent, ctx);
        if (c < 0)
        {
            slot = &parent->right;
        }
        else if (c > 0)
        {
            slot = &parent->left;
        }
        else
        {
            break;
        }
    }

    *out_slot = slot;
    return parent;
}

void Avl_Insert(AvlTree *tree, AvlLink *parent, AvlLink **slot, AvlLink *link)
{
    link->left = 0;
    link->right = 0;
    link->parent = parent;
    link->height = 0;
    *slot = link;
    tree->count++;

    Avl_Rebalance(tree, parent);
}

void Avl_Remove(AvlTree *tree, AvlLink *link)
{
    if (link->left != 0 && link->right != 0)
    {
        AvlLink *succ = link->right;
        while (succ->left != 0)
        {
            succ = succ->left;
        }
        Avl_SwapEntries(tree, link, succ);
    }

    // link now has at most one child
    AvlLink *child = (link->left != 0) ? link->left : link->right;
    AvlLink *rebalance_from = link->parent;

    Avl_ReplaceChild(tree, link->parent, link, child);

    tree->count--;

    link->left = 0;
    link->right = 0;
    link->parent = 0;
    link->height = 0;

    Avl_Rebalance(tree, rebalance_from);
}

AvlLink *Avl_Min(AvlTree *tree)
{
    AvlLink *n = tree->root;
    if (n == 0)
    {
        return 0;
    }
    while (n->left != 0)
    {
        n = n->left;
    }
    return n;
}

AvlLink *Avl_Max(AvlTree *tree)
{
    AvlLink *n = tree->root;
    if (n == 0)
    {
        return 0;
    }
    while (n->right != 0)
    {
        n = n->right;
    }
    return n;
}

AvlLink *Avl_Next(AvlLink *link)
{
    if (link->right != 0)
    {
        link = link->right;
        while (link->left != 0)
        {
            link = link->left;
        }
        return link;
    }

    while (link->parent != 0 && link == link->parent->right)
    {
        link = link->parent;
    }
    return link->parent;
}

AvlLink *Avl_Prev(AvlLink *link)
{
    if (link->left != 0)
    {
        link = link->left;
        while (link->right != 0)
        {
            link = link->right;
        }
        return link;
    }

    while (link->parent != 0 && link == link->parent->left)
    {
        link = link->parent;
    }
    return link->parent;
}
