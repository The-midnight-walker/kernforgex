// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_LISTS_H
#define INCLUDE_LISTS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>

/**
 * @struct list_node
 * @brief Intrusive doubly linked list node structure.
 */
typedef struct list_node {
    struct list_node *next; /**< Pointer to the next node in the list */
    struct list_node *prev; /**< Pointer to the previous node in the list */
} list_node_t;

/**
 * @brief Initializes a list node to point to itself (empty list head or
 * unlinked node).
 *
 * @param[in,out] node Pointer to the list node to initialize.
 */
static inline void list_init(list_node_t *node)
{
    node->next = node;
    node->prev = node;
}

/**
 * @brief Inserts a new node at the tail of the intrusive doubly linked list.
 *
 * @param[in,out] head     Pointer to the list head node.
 * @param[in,out] new_node Pointer to the new list node to insert.
 */
static inline void list_add_tail(list_node_t *head, list_node_t *new_node)
{
    list_node_t *prev = head->prev;
    new_node->next = head;
    new_node->prev = prev;
    prev->next = new_node;
    head->prev = new_node;
}

/**
 * @brief Removes a node from whatever intrusive doubly linked list it is
 * currently part of.
 *
 * @param[in,out] node Node to detach. Safe to call on a node that was never
 * inserted (i.e. still self-referencing from list_init()); it is a no-op.
 */
static inline void list_del(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

/**
 * @brief Casts a member pointer back to its enclosing container structure.
 *
 * @param ptr    Pointer to the structure member.
 * @param type   Data type of the container structure.
 * @param member Name of the member field inside the container structure.
 *
 * @return Pointer to the enclosing container structure.
 */
#define container_of(ptr, type, member)                                        \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief Iterates over an intrusive doubly linked list.
 *
 * @param pos  Pointer used as a loop cursor (list_node_t *).
 * @param head Pointer to the head node of the list.
 */
#define list_for_each(pos, head)                                               \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#endif /* INCLUDE_LISTS_H */