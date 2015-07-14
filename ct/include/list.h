/*  
* Created on 2010-11-01  
* filename: list.h 
* @file Simple doubly linked list implementation.
* Copyright: Copyright(c)2010 
* Company: WSN 
*/

#ifndef __LIST_H
#define __LIST_H


/* Basic type for the double-link list.  */
struct list_head {
	struct list_head *next, *prev;
};

/* Define a variable with the head and tail of the list.  */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/* Initialize a new list head.  */
#define LIST_HEADER(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * @brief  Insert a new entry after the specified head.
 * @author Lei Li  
 * @version 0.1  
 * @param new: new entry to be added
 * @param head: list head to add it after
 * @return void
 * @retval void
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * @brief  Insert a new entry before the specified head.
 * @author Lei Li  
 * @version 0.1  
 * @param new: new entry to be added
 * @param head: list head to add it before
 * @return void
 * @retval void
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

  /**
 * @brief  Delete a list entry by making the prev/next entries point to each other.
 * @author Lei Li  
 * @version 0.1  
 * @param entry: the element to delete from the list.
 * @return void
 * @retval void
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = (void *) 0;
	entry->prev = (void *) 0;
}

   /**
 * @brief  deletes entry from list and reinitialize it.
 * @author Lei Li  
 * @version 0.1  
 * @param entry: the element to delete from the list.
 * @return void
 * @retval void
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry); 
}

 /**
 * @brief  delete from one list and add as another's head
 * @author Lei Li  
 * @version 0.1  
 * @param list: the entry to move
 * @param head: the head that will precede our entry
 * @return void
 * @retval void
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
        __list_del(list->prev, list->next);
        list_add(list, head);
}

 /**
 * @brief  list_move_tail - delete from one list and add as another's tail
 * @author Lei Li  
 * @version 0.1  
 * @param list: the entry to move
 * @param head: the head that will follow our entry
 * @return void
 * @retval void
 */
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}

  /**
 * @brief list_empty - tests whether a list is empty
 * @author Lei Li  
 * @version 0.1  
 * @param head: the list to test.
 * @return void
 * @retval void
 */
static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}

static inline void __list_splice(struct list_head *list,
				 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

  /**
 * @brief  list_splice - join two lists
 * @author Lei Li  
 * @version 0.1  
 * @param list: the new list to add.
 * @param head: the place to add it in the first list.
 * @return void
 * @retval void
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

/**
 * @brief  list_splice - join two lists and reinitialise the emptied list.
 * @author Lei Li  
 * @version 0.1  
 * @param list: the new list to add.
 * @param head: the place to add it in the first list.
 * @return void
 * @retval void
 */
static inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


/* Iterate forward over the elements of the list.  */
#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

	
/* Iterate forward over the elements of the list.  */
#define list_for_each_prev(pos, head) \
  for (pos = (head)->prev; pos != (head); pos = pos->prev)


/* Iterate backwards over the elements list.  The list elements can be
   removed from the list while doing this.  */
#define list_for_each_prev_safe(pos, p, head) \
  for (pos = (head)->prev, p = pos->prev; \
       pos != (head); \
       pos = p, p = pos->prev)
        	


#endif

