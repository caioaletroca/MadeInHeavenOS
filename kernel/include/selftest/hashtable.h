#ifndef _SELFTEST_HASHTABLE_H_
#define _SELFTEST_HASHTABLE_H_

/**
 * Self-test for the intrusive hash table and list_move_after: one bucket by
 * hand (every pprev after inserts and removes at the front, middle and back), a
 * table of colliding keys found by comparing keys, hash_64 staying below 2^bits
 * and spreading consecutive keys, and the LRU move. Panics on failure.
 */
void hashtable_selftest(void);

#endif // _SELFTEST_HASHTABLE_H_
