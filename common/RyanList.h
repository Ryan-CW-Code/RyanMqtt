#ifndef __RyanMqttList__
#define __RyanMqttList__

#ifdef __cplusplus
extern "C"
{
#endif

// 计算结构体成员偏移量的宏
#define RyanOffsetOf(type, member) ((size_t)&(((type *)0)->member))

// 通过成员指针获取包含该成员的结构体指针
#define RyanContainerOf(ptr, type, member) \
    ((type *)((unsigned char *)(ptr) - RyanOffsetOf(type, member)))

// 通过链表节点获取所属结构体的首地址
#define RyanListEntry(list, type, member) \
    RyanContainerOf(list, type, member)

// 获取链表第一个元素所属结构体
#define RyanListFirstEntry(list, type, member) \
    RyanListEntry((list)->next, type, member)

// 获取链表最后一个元素所属结构体
#define RyanListPrevEntry(list, type, member) \
    RyanListEntry((list)->prev, type, member)

// 正向遍历链表
#define RyanListForEach(curr, list) \
    for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

// 反向遍历链表
#define RyanListForEachPrev(curr, list) \
    for ((curr) = (list)->prev; (curr) != (list); (curr) = (curr)->prev)

// 安全的正向遍历链表（支持遍历中删除）
#define RyanListForEachSafe(curr, next, list)          \
    for ((curr) = (list)->next, (next) = (curr)->next; \
         (curr) != (list);                             \
         (curr) = (next), (next) = (curr)->next)

// 安全的反向遍历链表（支持遍历中删除）
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
