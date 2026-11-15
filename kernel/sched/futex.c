#include <sched/futex.h>
#include <sched/wait.h>
#include <sched/spinlock.h>
#include <sys/hashtable.h>
#include <mm/address_space.h>
#include <mihos/errno.h>
#include <cleanup.h>
#include <stdbool.h>

#define FUTEX_BUCKET_BITS 6

/*
 * A thread asleep in futex_wait: on its own kernel stack, in the bucket its key hashes to,
 * until a wake takes it out or it leaves by itself (time limit, signal).
 */
typedef struct futex_waiter
{
    hlist_node_t bucket_link;
    struct address_space *space; // the key: an address means nothing without its space
    uintptr_t address;
    wait_queue_t queue; // only this thread sleeps on it
    bool woken;         // set by futex_wake, under futex_lock
} futex_waiter_t;

// One lock for the whole table: enough on one CPU (per-bucket locks for SMP)
static spinlock_t futex_lock = SPINLOCK_INIT;

// Zeroed buckets are empty: no init needed
static hlist_head_t buckets[HASH_BUCKETS(FUTEX_BUCKET_BITS)];

/**
 * The bucket a key hashes to. Different keys may share one: compare both halves.
 */
static hlist_head_t *futex_bucket(struct address_space *space, uintptr_t address)
{
    return &buckets[hash_64((uint64_t)(uintptr_t)space ^ address, FUTEX_BUCKET_BITS)];
}

int futex_wait(struct address_space *space, uintptr_t address, uint32_t expected, uint64_t ticks)
{
    if ((address & 3) != 0)
        return -EINVAL;

    futex_waiter_t waiter = {
        .space = space,
        .address = address};
    hlist_node_init(&waiter.bucket_link);
    wait_queue_init(&waiter.queue);

    guard(spinlock, &futex_lock);

    // Check and queue under one lock. A waker changes the word first and takes this lock to
    // wake: either the word differs already here, or the wake finds us queued. The read may
    // map a page touched for the first time, but never sleeps
    uint32_t value;
    if (address_space_read(space, address, &value, sizeof(value)) != 0)
        return -EFAULT;
    if (value != expected)
        return -EAGAIN;
    if (ticks == 0)
        return -ETIMEDOUT;

    hlist_insert(futex_bucket(space, address), &waiter.bucket_link);

    int result = ticks == FUTEX_NO_TIME_LIMIT ? wait_queue_sleep_interruptible(&waiter.queue, &futex_lock) : wait_queue_sleep_interruptible_timeout(&waiter.queue, &futex_lock, ticks);

    // A wake counts even when a signal or the time limit came too: the waker counted it, and
    // reporting EINTR would lose it (the lock it announced would never be taken)
    if (waiter.woken)
        return 0;

    hlist_remove(&waiter.bucket_link);
    return result;
}

size_t futex_wake(struct address_space *space, uintptr_t address, size_t count)
{
    size_t woken = 0;

    guard(spinlock, &futex_lock);

    hlist_node_t *node = futex_bucket(space, address)->first;
    while (node != NULL && woken < count)
    {
        hlist_node_t *next = node->next;
        futex_waiter_t *waiter = hlist_container(node, futex_waiter_t, bucket_link);

        if (waiter->space == space && waiter->address == address)
        {
            hlist_remove(&waiter->bucket_link);
            waiter->woken = true;
            wait_queue_wake_one(&waiter->queue);
            woken++;
        }

        node = next;
    }

    return woken;
}