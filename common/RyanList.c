
#include "RyanList.h"

/**
 * @brief 内部函数：在prev和next节点之间插入新节点
 *
 * @param node 要插入的新节点指针
 * @param prev 前驱节点指针
 * @param next 后继节点指针
 * @note 这是一个静态内部函数，不对外暴露
 */
static void _RyanListAdd(RyanList_t *node, RyanList_t *prev, RyanList_t *next)
{
    next->prev = node; // 后继节点的前驱指向新节点
    node->next = next; // 新节点的后继指向原后继节点
    node->prev = prev; // 新节点的前驱指向原前驱节点
    prev->next = node; // 前驱节点的后继指向新节点
}

/**
 * @brief 内部函数：删除prev和next之间的节点
 *
 * @param prev 要删除区间的前驱节点
 * @param next 要删除区间的后继节点
 * @note 这是一个静态内部函数，不对外暴露
 */
static void _RyanListDel(RyanList_t *prev, RyanList_t *next)
{
    prev->next = next; // 前驱节点直接指向后继节点
    next->prev = prev; // 后继节点直接指向前驱节点
}

/**
 * @brief 内部函数：删除指定节点自身
 *
 * @param entry 要删除的节点指针
 * @note 通过修改前后节点的指针关系实现自我删除
 */
static void _RyanListDel_entry(RyanList_t *entry)
{
    _RyanListDel(entry->prev, entry->next); // 调用区间删除函数
}

/**
 * @brief 初始化链表头节点
 *
 * @param list 链表头节点指针
 * @note 将头节点的前后指针都指向自己，形成空链表
 */
void RyanListInit(RyanList_t *list)
{
    list->next = list; // 后继指向自己
    list->prev = list; // 前驱指向自己
}

/**
 * @brief 在链表头部插入节点
 *
 * @param node 要插入的新节点
 * @param list 链表头节点
 * @note 新节点将插入到头节点之后，成为第一个有效节点
 */
void RyanListAdd(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list, list->next); // 在头节点和第一个节点间插入
}

/**
 * @brief 在链表尾部插入节点
 *
 * @param node 要插入的新节点
 * @param list 链表头节点
 * @note 新节点将插入到头节点之前，成为最后一个有效节点
 */
void RyanListAddTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListAdd(node, list->prev, list); // 在尾节点和头节点间插入
}

/**
 * @brief 从链表中删除指定节点
 *
 * @param entry 要删除的节点
 * @note 只删除节点，不重新初始化该节点
 */
void RyanListDel(RyanList_t *entry)
{
    _RyanListDel_entry(entry); // 调用内部删除函数
}

/**
 * @brief 从链表中删除并重新初始化指定节点
 *
 * @param entry 要删除的节点
 * @note 删除后会将该节点初始化为独立节点
 */
void RyanListDelInit(RyanList_t *entry)
{
    _RyanListDel_entry(entry); // 先删除节点
    RyanListInit(entry);       // 再重新初始化该节点
}

/**
 * @brief 将节点移动到链表头部
 *
 * @param node 要移动的节点
 * @param list 链表头节点
 * @note 先删除节点，再插入到头部
 */
void RyanListMove(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node); // 先从原位置删除
    RyanListAdd(node, list);  // 再插入到头部
}

/**
 * @brief 将节点移动到链表尾部
 *
 * @param node 要移动的节点
 * @param list 链表头节点
 * @note 先删除节点，再插入到尾部
 */
void RyanListMoveTail(RyanList_t *node, RyanList_t *list)
{
    _RyanListDel_entry(node);    // 先从原位置删除
    RyanListAddTail(node, list); // 再插入到尾部
}

/**
 * @brief 检查链表是否为空
 *
 * @param list 链表头节点
 * @return int 返回1表示空链表，0表示非空
 * @note 通过判断头节点是否指向自己来确定是否为空
 */
int RyanListIsEmpty(RyanList_t *list)
{
    return list->next == list; // 头节点的next指向自己说明为空
}
