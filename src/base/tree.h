#pragma once
#include "base/base_core.h"

enum NodeColor
{
    NODE_RED,
    NODE_BLACK
};

#define NODE_HEADER                \
    struct                         \
    {                              \
        struct NodeHeader *left;   \
        struct NodeHeader *right;  \
        struct NodeHeader *parent; \
        NodeColor color;           \
    }

struct NodeHeader
{
    struct NodeHeader *left;
    struct NodeHeader *right;
    struct NodeHeader *parent;
    NodeColor color;
};

struct Tree
{
    NodeHeader *root;
    U64 size;
};

#define tree_push(t, n, cmp, T) T##_tree_push(t, n, cmp)
#define tree_find(t, n, cmp, T) T##_tree_find(t, (T *)n, cmp)
#define tree_node(a, v, T)      T##_tree_node(a, v)

#define Tree_t(T)                                                                       \
    typedef struct T##_Node T##_Node;                                                   \
    struct T##_Node                                                                     \
    {                                                                                   \
        T##_Node *left;                                                                 \
        T##_Node *right;                                                                \
        T##_Node *parent;                                                               \
        NodeColor color;                                                                \
        T v;                                                                            \
    };                                                                                  \
                                                                                        \
    typedef struct                                                                      \
    {                                                                                   \
        T##_Node *root;                                                                 \
        U64 size;                                                                       \
    } T##_Tree;                                                                         \
                                                                                        \
    local_v T##_Node *T##_tree_node(Arena *a, T v)                                      \
    {                                                                                   \
        T##_Node *n = push_struct0(a, T##_Node);                                        \
        n->v = v;                                                                       \
        return n;                                                                       \
    }                                                                                   \
                                                                                        \
    local_v T##_Node *T##_tree_push(T##_Tree *t, T##_Node *n, S64 (*cmp)(T * a, T * b)) \
    {                                                                                   \
        T##_Node *cur = (T##_Node *)t->root;                                            \
        T##_Node *par = NULL;                                                           \
        while (cur)                                                                     \
        {                                                                               \
            par = cur;                                                                  \
            S64 res = cmp(&n->v, &cur->v);                                              \
            if (res == 0) { return cur; }                                               \
            else if (res > 0)                                                           \
            {                                                                           \
                cur = (T##_Node *)cur->right;                                           \
            }                                                                           \
            else                                                                        \
            {                                                                           \
                cur = (T##_Node *)cur->left;                                            \
            }                                                                           \
        }                                                                               \
        n->parent = par;                                                                \
        n->color = NODE_RED;                                                            \
        if (!par)                                                                       \
        {                                                                               \
            t->root = n;                                                                \
        }                                                                               \
        else if (cmp(&n->v, &par->v) > 0)                                               \
        {                                                                               \
            par->right = n;                                                             \
        }                                                                               \
        else                                                                            \
        {                                                                               \
            par->left = n;                                                              \
        }                                                                               \
        t->size++;                                                                      \
        return n;                                                                       \
    }                                                                                   \
                                                                                        \
    local_v T##_Node *T##_tree_find(T##_Tree *t, T *n, S64 (*cmp)(T * a, T * b))        \
    {                                                                                   \
        T##_Node *cur = t->root;                                                        \
        while (cur)                                                                     \
        {                                                                               \
            S64 res = cmp(n, &cur->v);                                                  \
            if (res == 0) { return cur; }                                               \
            else if (res > 0)                                                           \
            {                                                                           \
                cur = (T##_Node *)cur->right;                                           \
            }                                                                           \
            else                                                                        \
            {                                                                           \
                cur = (T##_Node *)cur->left;                                            \
            }                                                                           \
        }                                                                               \
        return (T##_Node *)NULL;                                                        \
    }
