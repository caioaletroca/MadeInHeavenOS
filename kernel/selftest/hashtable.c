#include <selftest/hashtable.h>
#include <sys/hashtable.h>
#include <sys/list.h>
#include <stdint.h>
#include <panic.h>

#define TABLE_BITS 2 // 4 buckets: 100 keys collide a lot, which is the point
#define TABLE_KEYS 100

typedef struct item
{
    uint64_t key;
    hlist_node_t node;
} item_t;

static item_t items[TABLE_KEYS];
static hlist_head_t table[HASH_BUCKETS(TABLE_BITS)]; // zeroed (static): empty

static item_t *table_find(uint64_t key)
{
    item_t *found = NULL;
    hlist_node_t *node;
    hlist_for_each(node, &table[hash_64(key, TABLE_BITS)])
    {
        item_t *item = hlist_container(node, item_t, node);
        if (item->key == key)
        {
            if (found != NULL)
                panic("Hash table self-test: key %u is in the table twice\n", (unsigned int)key);
            found = item;
        }
    }
    return found;
}

/**
 * One bucket by hand: every pprev must point at the pointer that points at its node.
 */
static void hashtable_test_bucket(void)
{
    hlist_head_t head = {NULL};
    item_t a, b, c;
    hlist_node_init(&a.node);
    hlist_node_init(&b.node);
    hlist_node_init(&c.node);
    if (hlist_hashed(&a.node))
        panic("Hash table self-test: a new node is hashed\n");

    // Each add goes to the front: c, b, a
    hlist_add_head(&head, &a.node);
    hlist_add_head(&head, &b.node);
    hlist_add_head(&head, &c.node);
    if (head.first != &c.node || c.node.next != &b.node || b.node.next != &a.node || a.node.next != NULL)
        panic("Hash table self-test: adds are not at the front\n");
    if (c.node.pprev != &head.first || b.node.pprev != &c.node.next || a.node.pprev != &b.node.next)
        panic("Hash table self-test: pprev after adds is wrong\n");
    if (!hlist_hashed(&a.node) || !hlist_hashed(&c.node))
        panic("Hash table self-test: an added node is not hashed\n");

    // The middle one: c, a
    hlist_del(&b.node);
    if (head.first != &c.node || c.node.next != &a.node || a.node.pprev != &c.node.next || hlist_hashed(&b.node))
        panic("Hash table self-test: deleting the middle node broke the bucket\n");

    // The first one: a
    hlist_del(&c.node);
    if (head.first != &a.node || a.node.pprev != &head.first || a.node.next != NULL || hlist_hashed(&c.node))
        panic("Hash table self-test: deleting the first node broke the bucket\n");

    // The last one: empty, and usable again
    hlist_del(&a.node);
    if (head.first != NULL || hlist_hashed(&a.node))
        panic("Hash table self-test: deleting the only node left something behind\n");
    hlist_add_head(&head, &b.node);
    if (head.first != &b.node || b.node.pprev != &head.first || b.node.next != NULL)
        panic("Hash table self-test: an emptied bucket does not take a node again\n");
}

/**
 * A whole table: keys share buckets, lookups compare keys, deletes leave the others.
 */
static void hashtable_test_table(void)
{
    for (uint64_t i = 0; i < TABLE_KEYS; i++)
    {
        items[i].key = i * 1000;
        hlist_node_init(&items[i].node);
        hlist_add_head(&table[hash_64(items[i].key, TABLE_BITS)], &items[i].node);
    }

    for (uint64_t i = 0; i < TABLE_KEYS; i++)
        if (table_find(i * 1000) != &items[i])
            panic("Hash table self-test: key %u is not found\n", (unsigned int)(i * 1000));
    if (table_find(1) != NULL || table_find(UINT64_MAX) != NULL)
        panic("Hash table self-test: a key never added was found\n");

    for (uint64_t i = 0; i < TABLE_KEYS; i += 2)
        hlist_del(&items[i].node);
    for (uint64_t i = 0; i < TABLE_KEYS; i++)
        if (table_find(i * 1000) != (i % 2 == 0 ? NULL : &items[i]))
            panic("Hash table self-test: after deleting the even keys, key %u is wrong\n", (unsigned int)(i * 1000));

    for (uint64_t i = 1; i < TABLE_KEYS; i += 2)
        hlist_del(&items[i].node);
    for (unsigned int b = 0; b < HASH_BUCKETS(TABLE_BITS); b++)
        if (table[b].first != NULL)
            panic("Hash table self-test: bucket %u is not empty after deleting everything\n", b);
}

/**
 * hash_64: always below 2^bits, and consecutive keys (block numbers) spread evenly.
 */
static void hashtable_test_hash(void)
{
    for (unsigned int bits = 1; bits <= 16; bits++)
        for (uint64_t key = 0; key < 1000; key++)
            if (hash_64(key, bits) >= (1ull << bits) || hash_64(key * 0x1000 + UINT64_MAX / 3, bits) >= (1ull << bits))
                panic("Hash table self-test: hash_64 of %u went past %u bits\n", (unsigned int)key, bits);

    if (hash_64(1, 64) != GOLDEN_RATIO_64)
        panic("Hash table self-test: hash_64 with 64 bits is not the plain product\n");

    // 1024 consecutive keys over 16 buckets: 64 each on average; multiplicative hashing gives nearly that
    unsigned int counts[16] = {0};
    for (uint64_t key = 0; key < 1024; key++)
        counts[hash_64(key, 4)]++;
    for (unsigned int b = 0; b < 16; b++)
        if (counts[b] < 48 || counts[b] > 80)
            panic("Hash table self-test: bucket %u got %u of 1024 consecutive keys\n", b, counts[b]);
}

/**
 * list_move_after: the LRU's "most recently used", from the back and from the middle.
 */
static void hashtable_test_list_move(void)
{
    list_t head, a, b, c;
    list_init(&head);
    list_insert_before(&head, &a);
    list_insert_before(&head, &b);
    list_insert_before(&head, &c); // a, b, c

    list_move_after(&head, &c); // c, a, b
    list_move_after(&head, &a); // a, c, b
    if (head.next != &a || a.next != &c || c.next != &b || b.next != &head)
        panic("Hash table self-test: list_move_after gave the wrong order\n");
    if (head.prev != &b || b.prev != &c || c.prev != &a || a.prev != &head)
        panic("Hash table self-test: list_move_after broke the back links\n");

    list_move_after(&head, &a); // already first: unchanged
    if (head.next != &a || a.prev != &head || a.next != &c || c.prev != &a)
        panic("Hash table self-test: moving the first item changed the list\n");
}

void hashtable_selftest(void)
{
    hashtable_test_bucket();
    hashtable_test_table();
    hashtable_test_hash();
    hashtable_test_list_move();
}
