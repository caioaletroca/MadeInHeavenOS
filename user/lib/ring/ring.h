#ifndef _RING_RING_H_
#define _RING_RING_H_

#include <stdbool.h>

/*
 * A ring buffer: a fixed array used as a circle, oldest item at data[start],
 * the next ones after it, wrapping past the end to data[0]. Removing the
 * oldest moves start, never the items. Header only: each macro becomes code
 * where it is used, for the ring's own item type.
 *
 * Every macro evaluates `self` once, like a function call. Zeroed, a ring is
 * empty:
 *
 *     typedef ring(char *, 32) lines_t;
 *     lines_t lines = {0};
 *     ring_push(&lines, "first");
 */

/**
 * @brief Define a ring buffer type.
 *
 * Each use is a new struct type, even with the same arguments: name it with
 * typedef to pass it to functions or declare it in more than one place.
 *
 * @param type The type of elements in the ring buffer.
 * @param capacity The maximum number of elements the ring buffer can hold.
 */
#define ring(type, capacity) \
    struct                   \
    {                        \
        type data[capacity]; \
        int start;           \
        int count;           \
    }

/**
 * @brief Get the capacity of the ring buffer.
 *
 * @param self The ring buffer.
 * @return The capacity of the ring buffer.
 */
#define ring_capacity(self) ((int)(sizeof((self)->data) / sizeof((self)->data[0])))

/**
 * @brief Check if the ring buffer is full.
 *
 * @param self The ring buffer.
 * @return True if the ring buffer is full, false otherwise.
 */
#define ring_is_full(self) ((self)->count == ring_capacity(self))

/**
 * @brief Check if the ring buffer is empty.
 *
 * @param self The ring buffer.
 * @return True if the ring buffer is empty, false otherwise.
 */
#define ring_is_empty(self) ((self)->count == 0)

/**
 * @brief Access an item at a specific index in the ring buffer. The item
 * itself, not a copy: it can be assigned to.
 *
 * @param self The ring buffer.
 * @param i 0 for the oldest item, up to count - 1 for the newest. Not checked:
 *          past count is a slot holding nothing, or a removed item.
 * @return The item at the specified index.
 */
#define ring_at(self, i)                                                    \
    (*({                                                                    \
        typeof(self) _ring_at = (self);                                     \
        &_ring_at->data[(_ring_at->start + (i)) % ring_capacity(_ring_at)]; \
    }))

/**
 * @brief Push an item into the ring buffer.
 *
 * @param self The ring buffer.
 * @param item The item to push into the buffer.
 * @return True if the item was added, false if the buffer is full.
 */
#define ring_push(self, item)                                \
    ({                                                       \
        typeof(self) _ring_self = (self);                    \
        bool _added = !ring_is_full(_ring_self);             \
        if (_added)                                          \
        {                                                    \
            ring_at(_ring_self, _ring_self->count) = (item); \
            _ring_self->count++;                             \
        }                                                    \
        _added;                                              \
    })

/**
 * @brief Shift (remove) the oldest item from the ring buffer, as JavaScript's
 * Array.shift. Not checked: the ring must not be empty.
 *
 * @param self The ring buffer.
 * @return The item that was removed from the buffer.
 */
#define ring_shift(self)                                                         \
    ({                                                                           \
        typeof(self) _ring_self = (self);                                        \
        typeof(_ring_self->data[0]) _item = ring_at(_ring_self, 0);              \
        _ring_self->start = (_ring_self->start + 1) % ring_capacity(_ring_self); \
        _ring_self->count--;                                                     \
        _item;                                                                   \
    })

#endif // _RING_RING_H_