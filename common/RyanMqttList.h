#ifndef __RyanMqttList__
#define __RyanMqttList__

#ifdef __cplusplus
extern "C" {
#endif

// 计算结构体成员偏移量的宏
#define RyanMqttOffsetOf(type, member) ((size_t)&(((type *)0)->member))

// 通过成员指针获取包含该成员的结构体指针
#define RyanMqttContainerOf(ptr, type, member) ((type *)((unsigned char *)(ptr) - RyanMqttOffsetOf(type, member)))

// 通过链表节点获取所属结构体的首地址
#define RyanMqttListEntry(list, type, member) RyanMqttContainerOf(list, type, member)

// 获取链表第一个元素所属结构体
#define RyanMqttListFirstEntry(list, type, member) RyanMqttListEntry((list)->next, type, member)

// 获取链表最后一个元素所属结构体
#define RyanMqttListPrevEntry(list, type, member) RyanMqttListEntry((list)->prev, type, member)

// 正向遍历链表
#define RyanMqttListForEach(curr, list) for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

// 反向遍历链表
#define RyanMqttListForEachPrev(curr, list) for ((curr) = (list)->prev; (curr) != (list); (curr) = (curr)->prev)

// 安全的正向遍历链表（支持遍历中删除）
#define RyanMqttListForEachSafe(curr, next, list)                                                                      \
	for ((curr) = (list)->next, (next) = (curr)->next; (curr) != (list); (curr) = (next), (next) = (curr)->next)

// 安全的反向遍历链表（支持遍历中删除）
#define RyanMqttListForEachPrevSafe(curr, next, list)                                                                  \
	for ((curr) = (list)->prev, (next) = (curr)->prev; (curr) != (list); (curr) = (next), (next) = (curr)->prev)

// 定义枚举类型

// 定义结构体类型
typedef struct RyanMqttListNode
{
	struct RyanMqttListNode *next;
	struct RyanMqttListNode *prev;
} RyanMqttList_t;

/* extern variables-----------------------------------------------------------*/

extern void RyanMqttListInit(RyanMqttList_t *list);

extern void RyanMqttListAdd(RyanMqttList_t *node, RyanMqttList_t *list);
extern void RyanMqttListAddTail(RyanMqttList_t *node, RyanMqttList_t *list);

extern void RyanMqttListDel(RyanMqttList_t *entry);
extern void RyanMqttListDelInit(RyanMqttList_t *entry);

extern void RyanMqttListMove(RyanMqttList_t *node, RyanMqttList_t *list);
extern void RyanMqttListMoveTail(RyanMqttList_t *node, RyanMqttList_t *list);

extern int RyanMqttListIsEmpty(RyanMqttList_t *list);

#ifdef __cplusplus
}
#endif

#endif
