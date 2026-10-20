#ifndef _SYS_HASH_TABLE_H_
#define _SYS_HASH_TABLE_H_

#include <sys/cdefs.h>
#include <stdint.h>
#include <string.h>

__BEGIN_DECLS

// A table of 2^bits buckets: hlist_head_t name[HASH_BUCKETS(bits)]; zeroed (static, kzalloc) it is empty
#define HASH_BUCKETS(bits) (1u << (bits))

// 2^64 divided by the golden ratio, made odd: multiplying by it spreads every input bit into the high bits
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

/*
 * Intrusive hash table (Linux's hlist). Embed an hlist_node_t in your struct,
 * keep an array of HASH_BUCKETS(bits) hlist_head_t, and pick a bucket with
 * hash_64(key, bits). The table stores no keys: walk the one bucket with
 * hlist_for_each and compare them yourself (different keys share buckets).
 *
 * A bucket is one pointer (half a list_t), so tables stay small; pprev points
 * at whatever points at the node, so deleting needs no head and no walk.
 */

typedef struct hlist_node
{
    struct hlist_node *next;   // the next node in the bucket, or NULL
    struct hlist_node **pprev; // the pointer that points at this node (a head's first or a node's next); NULL: not in a table
} hlist_node_t;

typedef struct hlist_head
{
    hlist_node_t *first;
} hlist_head_t;

/**
 * @brief Hashes a 64-bit value into `bits` bits: the top bits of a multiplicative hash.
 *
 * @param value The key (unsigned: the multiply may overflow, which is defined only for unsigned).
 * @param bits The table's size as a power of two, 1..64.
 * @return A bucket index below 2^bits.
 */
static inline uint64_t hash_64(uint64_t value, unsigned int bits)
{
    return (value * GOLDEN_RATIO_64) >> (64 - bits);
}

/**
 * @brief Marks a node as not in any table.
 */
static inline void hlist_node_init(hlist_node_t *node)
{
    node->next = NULL;
    node->pprev = NULL;
}

/**
 * @brief Whether a node is in a table.
 */
static inline int hlist_linked(const hlist_node_t *node)
{
    return node->pprev != NULL;
}

/**
 * @brief Puts a node at the front of a bucket.
 *
 * @param head The bucket to insert the node into.
 * @param node The node to insert (at the front: order in a bucket does not matter).
 */
static inline void hlist_insert(hlist_head_t *head, hlist_node_t *node)
{
    node->next = head->first;
    node->pprev = &head->first;
    if (head->first != NULL)
    {
        head->first->pprev = &node->next;
    }
    head->first = node;
}

/**
 * @brief Takes a node out of its bucket; afterwards it is in no table.
 *
 * @param node The node to remove from its bucket.
 */
static inline void hlist_remove(hlist_node_t *node)
{
    if (node->pprev != NULL)
    {
        *(node->pprev) = node->next;
        if (node->next != NULL)
        {
            node->next->pprev = node->pprev;
        }
        node->next = NULL;
        node->pprev = NULL;
    }
}

// Walks one bucket: node is an hlist_node_t *
#define hlist_for_each(node, head) for (node = (head)->first; node != NULL; node = node->next)

// The struct an hlist_node_t is embedded in (list_container for hlists)
#define hlist_container(node, type, member) ((type *)((char *)(node) - offsetof(type, member)))

__END_DECLS

#endif