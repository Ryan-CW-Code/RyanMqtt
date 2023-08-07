

#include "RyanList.h"

/**
 * @brief 在prev和next之前插入节点
 *
 * @param node
 * @param prev
 * @param next
 */
static void _RyanListAdd(RyanList_t *node, RyanList_t *prev, RyanList_t *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

/**
 * @brief 删除prev和next之间的节点
 *
 * @param prev
 * @param next
 */
static void _RyanListDel(RyanList_t *prev, RyanList_t *next)
{
    prev->next = next;
    next->prev = prev;
}

/**
 * @brief 删除自己
 *
 * @param entry
 */
static void _RyanListDel_entry(RyanList_t *entry)
{
    _RyanListDel(entry->prev, entry->next);
}

/**
 * @brief 初始链表
 *
 * @param list
 */
void RyanListInit(RyanList_t *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * @brief 链表头插
 *
 * @param node
 * @param list
 */
void RyanListAdd(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list, list->next);
}

/**
 * @brief 链表尾插
 *
 * @param node
 * @param list
 */
void RyanListAddTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list->prev, list);
}

/**
 * @brief 删除自己
 *
 * @param entry
 */
void RyanListDel(RyanList_t *entry)
{
    _RyanListDel_entry(entry);
}

/**
 * @brief 删除自己
 *
 * @param entry
 */
void RyanListDelInit(RyanList_t *entry)
{
    _RyanListDel_entry(entry);
    RyanListInit(entry);
}

/**
 * @brief 将节点移到链表头部
 *
 * @param node
 * @param list
 */
void RyanListMove(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node);
    RyanListAdd(node, list);
}

/**
 * @brief 将节点移到链表尾部
 *
 * @param node
 * @param list
 */
void RyanListMoveTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node);
    RyanListAddTail(node, list);
}

/**
 * @brief 链表是否为空
 *
 * @param list
 * @return int
 */
int RyanListIsEmpty(RyanList_t *list)
{
    return list->next == list;
}
