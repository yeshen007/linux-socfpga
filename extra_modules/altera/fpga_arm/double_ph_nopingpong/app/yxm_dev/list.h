#ifndef __F2SM_LIST_H__
#define __F2SM_LIST_H__

/* 将val写到地址var中 */
#define WRITE_ONCE(var, val) \
	(*((volatile typeof(val) *)(&(var))) = (val))

/* 读取var地址中的值 */
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

struct list_head {
	struct list_head *next, *prev;
};

/* 链表静态初始化 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

/* 链表动态初始化 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/* 将节点New插入到prev之后next之前 */
static inline void __list_add(struct list_head *New, struct list_head *prev, struct list_head *next)
{
	next->prev = New;
	New->next = next;
	New->prev = prev;
	WRITE_ONCE(prev->next, New);
}

/* 将节点New插入到head节点后 */
static inline void list_add(struct list_head *New, struct list_head *head)
{
	__list_add(New, head, head->next);
}

/* 将节点New插入到head之前 */
static inline void list_add_tail(struct list_head *New, struct list_head *head)
{
	__list_add(New, head->prev, head);
}


static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	WRITE_ONCE(prev->next, next);
}

/* 删除节点entry */
static inline void __list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/* 更干净的删除节点entry */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = NULL;
	entry->prev = NULL;
}

/* 检查队列head是否为空，为空返回1，不空返回0 */
static inline int list_empty(const struct list_head *head)
{
	return READ_ONCE(head->next) == head;
}

/* 获取包含链表节点的结构体的首地址
 * ptr是链表节点地址
 * type是包含链表节点的结构体类型
 * member是链表节点的在这个结构体类型中的名字
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member));	\
})

/* 和container_of一样 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)


/* ptr指向链表节点 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)


#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

/* pos指向父结构体 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)


#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)

/* pos和head都指向链表节点 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/* 从head为队列头开始遍历，pos和head都指向链表节点的父结构体 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_next_entry(pos, member))


#endif

