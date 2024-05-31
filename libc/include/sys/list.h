#ifndef _LIST_H_
#define _LIST_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Circularly double linked list data structure
*/
typedef struct list {
    // Link for the previous item
    struct list *prev;

    // Link fo the next item
    struct list *next;
} list_t;

/**
 * Initialize a list
 * The link is made a point to itself
 * 
 * @param list  List pointer
*/
static inline void list_init(list_t *list) {
    list->next = list;
    list->prev = list;
}

/**
 * Insert a new item into a list before the current list position
 * 
 * @param list List pointer to a item in the list,
 * the new item will be positioned before this item
 * @param node New item pointer
*/
static inline void list_insert_before(list_t *list, list_t *node) {
    node->next = list;
    node->prev = list->prev;
    list->prev->next = node;
    list->prev = node;
}

/**
 * Insert a new item into a list after the current list position
 * 
 * @param list  List pointer to a item in the list,
 * the new item will be positioned after this item
 * @param node  New item pointer
*/
static inline void list_insert_after(list_t *list, list_t *node) {
    node->next = list->next;
    node->prev = list;
    list->next->prev = node;
    list->next = node;
}

/**
 * Unlink a item from the list
 * After unlinked, the element is a single element list
 * 
 * @param node  The item to be removed
*/
static inline void list_delete(list_t *node) {
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
#define list_empty(list) ((list)->next == (list))

/**
 * Get a pointer to the struct start for this list element.
 *
 * @elem:   the struct list_link pointer.
 * @type:   the type of the struct the element is embedded in.
 * @member: the name of the list_link within the struct.
 */
#define list_container(link, type, member) \
    ((type *) ((char *)(link) - offsetof(type, member)))

/**
 * Get a pointer to the struct start for this list element.
 * Constant version.
 *
 * @elem:   the struct list_link pointer.
 * @type:   the type of the struct the element is embedded in.
 * @member: the name of the list_link within the struct.
 */
#define list_container_const(link, type, member) \
    ((const type *) ((const char *)(link) - offsetof(const type, member)))

__END_DECLS

#endif