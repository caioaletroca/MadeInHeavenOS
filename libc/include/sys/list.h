#ifndef _SYS_LIST_H_
#define _SYS_LIST_H_

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stddef.h>

__BEGIN_DECLS

/**
 * Circularly double linked list data structure
 */
typedef struct list
{
    // Link for the previous item
    struct list *prev;

    // Link for the next item
    struct list *next;
} list_t;

/**
 * Initialize a list
 * The link is made a point to itself
 *
 * @param list  List pointer
 */
static inline void list_init(list_t *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * Insert a new item into a list before the current list position
 *
 * @param position List pointer to a item in the list,
 * the new item will be positioned before this item
 * @param node New item pointer
 */
static inline void list_insert_before(list_t *position, list_t *node)
{
    node->next = position;
    node->prev = position->prev;
    position->prev->next = node;
    position->prev = node;
}

/**
 * Insert a new item into a list after the current list position
 *
 * @param position  List pointer to a item in the list,
 * the new item will be positioned after this item
 * @param node  New item pointer
 */
static inline void list_insert_after(list_t *position, list_t *node)
{
    node->next = position->next;
    node->prev = position;
    position->next->prev = node;
    position->next = node;
}

/**
 * Unlink a item from the list
 * After unlinked, the element is a single element list
 *
 * @param node  The item to be removed
 */
static inline void list_delete(list_t *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = node;
    node->prev = node;
}

/**
 * Check if the list is empty
 *
 * @param list  List pointer
 * @return      True if it is empty
 */
static inline bool list_empty(const list_t *list)
{
    return list->next == list;
}

/**
 * Iterate over each element in the list
 *
 * @node: Current node pointer
 * @head: Head of the list
 */
#define list_for_each(node, head) \
    for (node = (head)->next; node != (head); node = node->next)

/**
 * Iterate over each element in the list safely against removal of list entry
 *
 * @node: Current node pointer
 * @next: Temporary storage for the next node
 * @head: Head of the list
 */
#define list_for_each_safe(node, next, head) \
    for (node = (head)->next, next = node->next; node != (head); node = next, next = node->next)

/**
 * Get a pointer to the struct start for this list element.
 *
 * @link:   the struct list_link pointer.
 * @type:   the type of the struct the element is embedded in.
 * @member: the name of the list_link within the struct.
 */
#define list_container(link, type, member) \
    ((type *)((char *)(link) - offsetof(type, member)))

/**
 * Get a pointer to the struct start for this list element.
 * Constant version.
 *
 * @link:   the struct list_link pointer.
 * @type:   the type of the struct the element is embedded in.
 * @member: the name of the list_link within the struct.
 */
#define list_container_const(link, type, member) \
    ((const type *)((const char *)(link) - offsetof(type, member)))

__END_DECLS

#endif