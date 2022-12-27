
#ifndef __RyanList__
#define __RyanList__

#ifdef __cplusplus
extern "C"
{
#endif

#define RyanOffsetOf(type, member) ((size_t) & (((type *)0)->member))

#define RyanContainerOf(ptr, type, member) \
    ((type *)((unsigned char *)(ptr)-RyanOffsetOf(type, member)))

// 通过链表获取节点首地址
#define RyanListEntry(list, type, member) \
    RyanContainerOf(list, type, member)

// 从链表指针ptr的下一指针中获得包含该链表的结构体指针
#define RyanListFirstEntry(list, type, member) \
    RyanListEntry((list)->next, type, member)

// 从链表指针ptr的上一指针中获得包含该链表的结构体指针
#define RyanListPrevEntry(list, type, member) \
    RyanListEntry((list)->prev, type, member)

// 遍历链表正序
#define RyanListForEach(curr, list) \
    for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

// 遍历链表反序
#define RyanListForEachPrev(curr, list) \
    for ((curr) = (list)->prev; (curr) != (list); (curr) = (curr)->prev)

// 安全遍历链表正序
#define RyanListForEachSafe(curr, next, list)          \
    for ((curr) = (list)->next, (next) = (curr)->next; \
         (curr) != (list);                             \
         (curr) = (next), (next) = (curr)->next)

// 安全遍历链表反序
#define RyanListForEachPrevSafe(curr, next, list)      \
    for ((curr) = (list)->prev, (next) = (curr)->prev; \
         (curr) != (list);                             \
         (curr) = (next), (next) = (curr)->prev)

    // 定义枚举类型

    // 定义结构体类型
    typedef struct RyanListNode
    {
        struct RyanListNode *next;
        struct RyanListNode *prev;
    } RyanList_t;

    /* extern variables-----------------------------------------------------------*/

    extern void RyanListInit(RyanList_t *list);

    extern void RyanListAdd(RyanList_t *node, RyanList_t *list);
    extern void RyanListAddTail(RyanList_t *node, RyanList_t *list);

    extern void RyanListDel(RyanList_t *entry);
    extern void RyanListDelInit(RyanList_t *entry);

    extern void RyanListMove(RyanList_t *node, RyanList_t *list);
    extern void RyanListMoveTail(RyanList_t *node, RyanList_t *list);

    extern int RyanListIsEmpty(RyanList_t *list);

#ifdef __cplusplus
}
#endif

#endif
