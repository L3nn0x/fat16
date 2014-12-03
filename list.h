/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   list.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: availlan <availlan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2014/12/03 10:11:35 by availlan          #+#    #+#             */
/*   Updated: 2014/12/03 10:46:58 by availlan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef LIST_H
# define LIST_H

struct list_head
{
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void list_add(struct list_¨head *head struct list_head *new)
{
	new->next = head->next;
	new->prev = head;
	head->next->prev = new;
	head->next = new;
}

static inline void list_del(struct list_head *del)
{
	del->next->prev = del->prev;
	del->prev->next = del->next;
	del->prev = 0;
	del->next = 0;
}

static inline int list_empty(struct list_head *head)
{
	return head->next == head->prev;
}

#define list_entry(ptr, type, member) (type*)((char*)ptr - (char*)&((type*)0)->member)

#define list_first_entry(head, type, member) list_entry((head)->next, type, member)

#define list_for_each(head, p) for (p = (head)->next; p != (head); p = p->next)

#define list_for_each_safe(head, p, n) for (p = (head)->next, n = p->next; p != (head); p = n, n = p->next)

#define list_for_each_entry(head, p, member) for (p = list_entry((head)->next, typeof(*p), member); &p->member != (head);

#endif /* !LIST_H */
