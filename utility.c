#include "utility.h"

void display(struct Queue *q);

/* enqueue a thread at the end of the queue/list */
void enqueue(struct _MyThread *x, struct Queue *q) {
	struct ThreadNode *n = malloc(NodeSize);
	n->t = x;
	n->next = NULL;
	if(q->head == NULL) 
		q->head = n;
	else 
		q->tail->next = n;
	q->tail = n;
	//display(q);
}
    
/* dequeue a thread from the start of a queue/list */
struct _MyThread* dequeue(struct Queue *q) {
	struct _MyThread *m;
	struct ThreadNode *p;
	if(q->head == NULL)
		return NULL;
	else {
		p=q->head; //p is used to free the memory
		if(p->next != NULL)
			q->head = p->next;
		else
			q->head = q->tail = NULL;                                    
		m = p->t;
		free(p);
		//display(q);
		return m;    
    }
}

void display(struct Queue *q) {
	struct ThreadNode *p = q->head;
	while (p!=NULL) {
		printf("%d", p->t->thread_id);
		p = p->next;
	}
	printf("\n");
}

/* delete a thread at any position in a list 
 * Note: We are assuming that the given thread exists in the q->eue
 */
void delete_thread(struct _MyThread* x, struct Queue *q) {
	struct ThreadNode *s, *p;
	s = NULL;
	p = q->head; 
	while(p != NULL) {
		if(p->t == x) {
			if (s != NULL)
				s->next = p->next;
			else 
				q->head = p->next;

			if (p->next == NULL)
				q->tail = s;

			free(p);
			return;
		}
		s = p;
		p = p->next;                            
	}
}

/* Below functions are redundant...we can maintain a children queue data structure
 * and use enqueue, dequeue and delete_thread functions to manage it....too tired to change now
 */
void add_child(struct _MyThread *me, struct _MyThread *child) {
	struct ThreadNode *kid = (struct ThreadNode*) malloc(sizeof(struct ThreadNode));
	kid->t = child;
	kid->next = me->children;
	me->children = kid;
}

void delete_child(struct _MyThread *me, struct _MyThread *child) {
	struct ThreadNode *mychild = me->children, *temp;
	if (mychild == NULL) printf("WTF?\n");
	if (mychild->t == child) {
		me->children = mychild->next;
		free(mychild);
		return;
	}

    while (mychild->next != NULL) {
		temp = mychild;
        mychild = mychild->next;

        if (mychild->t == child) {
			temp->next = mychild->next;
			free(mychild);
			return; 
		}
    }
}

void update_parent_of_children(struct _MyThread *me) {
	struct ThreadNode *mychild, *temp;
    mychild = me->children;
    while (mychild != NULL) {
        mychild->t->parent = NULL;
        temp = mychild;
        mychild = mychild->next;
        free(temp);
    }
}

int is_child_active(struct _MyThread *me, struct _MyThread *child) {
	struct ThreadNode *mychild = me->children;
    while (mychild != NULL) {
        if (mychild->t == child)
			return 1;
        mychild = mychild->next;
    }
	return 0;
}

/* utility function to handle errors */
void handle_error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

