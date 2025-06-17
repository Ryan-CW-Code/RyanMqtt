#include "RyanList.h"

/**
 * @brief Inserts a node between two existing nodes in a doubly linked list.
 *
 * This internal static function updates the pointers of the given nodes to insert `node`
 * between `prev` and `next`. It does not perform any validation on the input pointers.
 *
 * @param node The node to insert.
 * @param prev The node that will precede the inserted node.
 * @param next The node that will follow the inserted node.
 */
static void _RyanListAdd(RyanList_t *node, RyanList_t *prev, RyanList_t *next)
{
    next->prev = node; // 后继节点的前驱指向新节点
    node->next = next; // 新节点的后继指向原后继节点
    node->prev = prev; // 新节点的前驱指向原前驱节点
    prev->next = node; // 前驱节点的后继指向新节点
}

/**
 * @brief Unlinks all nodes between two given nodes in a doubly linked list.
 *
 * Adjusts the pointers of the specified predecessor and successor nodes so that they point to each other, effectively removing any nodes between them from the list.
 */
static void _RyanListDel(RyanList_t *prev, RyanList_t *next)
{
    prev->next = next; // 前驱节点直接指向后继节点
    next->prev = prev; // 后继节点直接指向前驱节点
}

/**
 * @brief Removes a specific node from the doubly linked list.
 *
 * This internal function unlinks the given node from its neighboring nodes, effectively deleting it from the list.
 *
 * @param entry Pointer to the node to be removed.
 */
static void _RyanListDel_entry(RyanList_t *entry)
{
    _RyanListDel(entry->prev, entry->next); // 调用区间删除函数
}

/**
 * @brief Initializes a list head node to represent an empty doubly linked list.
 *
 * Sets the head node's next and previous pointers to point to itself, marking the list as empty.
 */
void RyanListInit(RyanList_t *list)
{
    list->next = list; // 后继指向自己
    list->prev = list; // 前驱指向自己
}

/**
 * @brief Inserts a node at the beginning of the list, immediately after the head.
 *
 * The new node becomes the first valid element in the doubly linked list.
 */
void RyanListAdd(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list, list->next); // 在头节点和第一个节点间插入
}

/**
 * @brief Inserts a node at the end of the doubly linked list.
 *
 * The new node is added immediately before the list head, making it the last valid node in the list.
 */
void RyanListAddTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list->prev, list); // 在尾节点和头节点间插入
}

/**
 * @brief Removes a specified node from the doubly linked list.
 *
 * The node is unlinked from its current list but is not reinitialized; its pointers remain unchanged after removal.
 */
void RyanListDel(RyanList_t *entry)
{
    _RyanListDel_entry(entry); // 调用内部删除函数
}

/**
 * @brief Removes a node from the list and reinitializes it as a standalone node.
 *
 * After removal, the node's `next` and `prev` pointers are set to point to itself, making it an isolated node.
 */
void RyanListDelInit(RyanList_t *entry)
{
    _RyanListDel_entry(entry); // 先删除节点
    RyanListInit(entry);       // 再重新初始化该节点
}

/**
 * @brief Moves a node to the front of the list.
 *
 * Removes the specified node from its current position and inserts it immediately after the list head, making it the first element in the list.
 */
void RyanListMove(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node); // 先从原位置删除
    RyanListAdd(node, list);  // 再插入到头部
}

/**
 * @brief Moves a node to the end of the doubly linked list.
 *
 * Removes the specified node from its current position and inserts it immediately before the list head, making it the last node in the list.
 *
 * @param node The node to move.
 * @param list The list head node.
 */
void RyanListMoveTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node);    // 先从原位置删除
    RyanListAddTail(node, list); // 再插入到尾部
}

/**
 * @brief Checks whether the list is empty.
 *
 * Determines if the given list head node represents an empty list by checking if its `next` pointer points to itself.
 *
 * @param list Pointer to the list head node.
 * @return int Returns 1 if the list is empty, 0 otherwise.
 */
int RyanListIsEmpty(RyanList_t *list)
{
    return list->next == list; // 头节点的next指向自己说明为空
}
