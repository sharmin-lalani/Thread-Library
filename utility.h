#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>

struct ThreadNode;

/* This is an internal implementation used by the thread library.
 * All routines accept a public thread handle as an argument,
 * which is then cast to the internal format for processing,
 * and back to the public format on return.
 * Look for the handle in MyThread.h
 */
struct _MyThread {
    int thread_id;
    ucontext_t uctx;
    struct _MyThread* parent;
	struct ThreadNode *children;
    int no_of_active_children;
    int blocked_on_child; /* This field takes the following values:
                           * -1: not blocked on any child
                           *  0: blocked on all children
                           * >0: blocked on a specific child whose thread_id is given by the value
                           */
};

/* This structure will be used to implement both,
 * the ready queue and the blocked list.
 */
struct ThreadNode {
    struct _MyThread *t;
    struct ThreadNode *next;
};

#define NodeSize sizeof(struct ThreadNode)

struct Queue {
    struct ThreadNode *head, *tail;
};

struct _MySemaphore {
    int value;
    struct Queue BlockQ;
};

void enqueue(struct _MyThread *x, struct Queue *q);

struct _MyThread* dequeue(struct Queue *q);

void delete_thread(struct _MyThread* x, struct Queue *q);

void add_child(struct _MyThread *me, struct _MyThread *child);

void delete_child(struct _MyThread *me, struct _MyThread *child);

void update_parent_of_children(struct _MyThread *me);

int is_child_active(struct _MyThread *me, struct _MyThread *child);

