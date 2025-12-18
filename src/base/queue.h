#pragma once

#define _SLLQueueHeader_(n, t) \
	struct n                   \
	{                          \
		t *head;               \
		t *tail;               \
		U64 size;              \
	}

//#define sllqueue_init(q, n) (q->head = n, q->tail = n, q->size = 1)
#define sllqueue_push(q, n) ((n)->next = (q)->head, (n)->prev = NULL, ((q)->head) ? ((q)->head->prev = (n)) : ((q)->tail = (n)), (q)->head = (n), (q)->size++)
#define sllqueue_pop(q, n)	((sllqueue_empty(q) ? ((n) = NULL) : ((n) = (q)->tail, (q)->tail = (q)->tail->prev, (q)->tail ? (q)->tail->next = NULL : (q)->head = NULL), (q)->size--))
#define sllqueue_empty(q)	((q)->size == 0)

#define BUFFER_QUEUE(name, t, s) \
	struct                       \
		buffer_queue_##t         \
	{                            \
		U64 head;                \
		U64 tail;                \
		U64 size;                \
		t v[s];                  \
	} name = {(t)0}

#define bufferqueue_push(q, t) ((q).tail %= StaticArraySize((q).v), (q).size++, (q).v[(q).tail++] = (t))
#define bufferqueue_empty(q)   ((q).size == 0)
#define bufferqueue_full(q)	   ((q).size == StaticArraySize(q.v))
#define bufferqueue_pop(q)	   ((q).head %= StaticArraySize((q).v), (bufferqueue_empty((q)) ? ((Task *)NULL) : (((q).size--), (q).v + (q).head++)))
#define bufferqueue_front(q)   ((q).head %= StaticArraySize((q).v), (bufferqueue_empty((q)) ? (NULL) : ((q).v + (q).head)))
