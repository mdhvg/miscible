#pragma once

#include "base/core.h"
#include "base/arena.h"

#define _QueueHeader_(n, t) \
	struct n                \
	{                       \
		t *head;            \
		t *tail;            \
		U64 count;          \
	}

//#define queue_init(q, n) (q->head = n, q->tail = n, q->count = 1)
#define queue_push(q, n) ((n)->next = (q)->head, (n)->prev = NULL, ((q)->head) ? ((q)->head->prev = (n)) : ((q)->tail = (n)), (q)->head = (n), (q)->count++)
#define queue_pop(q, n)	 ((queue_empty(q) ? ((n) = NULL) : ((n) = (q)->tail, (q)->tail = (q)->tail->prev, (q)->tail ? (q)->tail->next = NULL : (q)->head = NULL), (q)->count--))
#define queue_empty(q)	 ((q)->count == 0)