#ifndef _SYS_HASH_TABLE_H_
#define _SYS_HASH_TABLE_H_

#include <sys/cdefs.h>
#include <stdint.h>
#include <string.h>

__BEGIN_DECLS

/**
 * Useful resources:
 * 
 * Integer Hash Function
 * https://gist.github.com/badboy/6267743
*/

/**
 * Hash table structure
*/
typedef struct hash_table {
    // Pointer to the next item
    struct hash_table *next;

    // A Double pointer to the previous item
    struct hash_table **pprev;
} hash_table_t;

/**
 * Hashes a 32 bit key
 * 
 * @param value         Value to be hashed
*/
static inline unsigned int hash_32(int key) {
  key = (key ^ 61) ^ (key >> 16);
  key = key + (key << 3);
  key = key ^ (key >> 4);
  key = key * 0x27d4eb2d; // a prime or an odd constant
  key = key ^ (key >> 15);

  return key;
}

/**
 * Hashes a 64 bit key
 * 
 * @param value         Value to be hashed
*/
static inline unsigned long hash_64(long key) {
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);

  return key;
}

/**
 * Hashes a key into a table index
 * 
 * @param value     Numeric key to be hashed
*/
#define hash(value) \
    ((sizeof(value) <= 4) ? hash_32(value) : hash_64(value))

/**
 * Initializes a hash table
 * 
 * @param table     Hash table double pointer
*/
static inline void hash_table_init(hash_table_t **table) {
    memset(table, 0, sizeof(hash_table_t));
}

/**
 * Inserts a new node into a hash table using a given key
 * 
 * @param table     Hash table double pointer
 * @param node      Node pointer
 * @param key       Key for the node
*/
static inline void hash_table_insert(hash_table_t **table, hash_table_t *node, long long key) {
    int index = hash(key);
    node->next = table[index];
    node->pprev = &table[index];

    if(table[index] != NULL) {
        table[index]->pprev = (hash_table_t **)node;
    }

    table[index] = node;
}

/**
 * Deletes a node from a hash table
 * 
 * @param node      Node pointer
*/
static inline void hash_table_delete(hash_table_t *node) {
    hash_table_t *next = node->next;
    hash_table_t **pprev = node->pprev;

    *pprev = next;
    if(next != NULL) {
        next->pprev = pprev;
    }
}

/**
 * Gets the value from the hash table by a key
 * 
 * @param table     Hash table double pointer
 * @param key       Key for the node
 * @return          The node found
*/
static inline hash_table_t hash_table_lookup(hash_table_t * const *table, long long key) {
    return table[hash(key)];
}

__END_DECLS

#endif