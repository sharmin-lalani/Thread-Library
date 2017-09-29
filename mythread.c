/******************************************************************************
 *
 *  File Name........: mythread.c
 *
 *  Description......: Implement a non pre‐emptive user‐level threads library
 *
 *****************************************************************************/

#include "utility.h"
#include "mythread.h"

#define THREAD_STACK_SIZE 8192 // our thread stack is 8KB in size
int no_of_threads = 0; // number of threads in the process
struct _MyThread *curr_thread = NULL; /* curr_thread is updated every time we call swapcontext or setcontext.
									   * We are assuming this library will only execute in a single‐threaded (OS) process. 
									   * An internal thread operation cannot be interrupted by another thread operation.
									   * E.g., MyThreadCreate will not be interrupted by MyThreadYield.
									   * That means the library does not have to acquire locks to protect 
									   * the internal data of the thread library.
									   * A user may still have to acquire semaphores to protect user data.
									   */
ucontext_t init_context;


struct Queue ReadyQ = {NULL,NULL}, BlockL = {NULL,NULL};

/* Allocate memory and populate fields for a new struct _MyThread variable.
 * Call getcontext() to initialize the ucontext_t.
 * allocate memory for the stack and set the stack pointer in the ucontext_t. 
 */
struct _MyThread* make_new_thread(void) {
		struct _MyThread *t = (struct _MyThread *)malloc(sizeof(struct _MyThread));
		ucontext_t *exit_context = (ucontext_t*) malloc(sizeof(ucontext_t));
		char *t_stack = (char *)malloc(THREAD_STACK_SIZE);
		char *exit_stack = (char *)malloc(THREAD_STACK_SIZE);
		if (t == NULL || t_stack == NULL || exit_stack == NULL)
				handle_error("failed to allocate memory for thread");

		t->thread_id = ++no_of_threads; /* Derive thread_id from a global thread counter
										 * Note: Thread counter has to start from 1 as 0 is reserved for a special purpose.
										 */

		t->parent = curr_thread; // For the initial thread, parent will be NULL.
		t->no_of_active_children = 0;
		t->blocked_on_child = -1;
		t->children = NULL;

		if (curr_thread != NULL) {
				curr_thread->no_of_active_children++;
				add_child(curr_thread, t);
		}

		if (getcontext(&t->uctx) == -1 || getcontext(exit_context) == -1)
				handle_error("getcontext failed");

		t->uctx.uc_stack.ss_sp = t_stack;
		t->uctx.uc_stack.ss_size = THREAD_STACK_SIZE;
		exit_context->uc_stack.ss_sp = exit_stack;
		exit_context->uc_stack.ss_size = THREAD_STACK_SIZE;
		t->uctx.uc_link = exit_context; /* This field points to the next context to execute after the current context.
										 * We want the Thread to execute the exit routine after executing start_funct.
										 */
		exit_context->uc_link = NULL;	
		return t;
} 

/* Create a new thread.
 * Call makecontext() to initialize the context with a starting function.
 * Enqueue the newly created thread context to the ready queue.
 * return the Mythread structure.
 *
 * Usage: The parameter start_func is the function in which the new thread starts executing. 
 * The parameter args is passed to the start function.
 * The interface only allows one parameter (a void *) to be passed to a MyThread. 
 * This is sufficient. One can build wrapper functions that pack and unpack an arbitrary parameter
 * list into an object pointed to by a void *.
 */
MyThread MyThreadCreate(void(*start_funct)(void *), void *args) {
		struct _MyThread *t = make_new_thread();

		makecontext(&t->uctx, start_funct, 1, args);
		makecontext(t->uctx.uc_link, MyThreadExit, 0);
		//printf("child created with thread id %d\n", t->thread_id);
		enqueue(t, &ReadyQ);
		return (MyThread) t;
}

/* Yield invoking thread:
 * Fetch the next available thread context from the ready queue.
 * If ready queue is empty, continue executing current context;
 * Else call getcontext() to store the context for the current thread, 
 * enqueue current thread context to the ready queue,
 * and call setcontext() to switch to the fetched thread context. 
 */
void MyThreadYield(void) {
		struct _MyThread *prev = curr_thread;
		enqueue(curr_thread, &ReadyQ);	
		curr_thread = dequeue(&ReadyQ);

		//printf("yield called, next thread to run: %d\n",  curr_thread->thread_id);
		if (swapcontext(&prev->uctx, &curr_thread->uctx) == -1)
				handle_error("swapcontext failed");
}

/* Join with a child thread:
 * Joins the invoking function with the specified child thread,
 * i.e. block the invoking thread till the specified child thread has terminated.
 * If the specified thread is not an immediate child of invoking thread,
 * return -1 to denote failure.
 * If the specified child has already terminated, do not block and 
 * return 0 to denote success;
 * else add the current thread context to the blocked thread list,
 * and swap the current thread with the next available thread on the ready queue.
 * Once execution flow returns back to this thread context, 
 * return 0 to denote success. 
 */
int MyThreadJoin(MyThread thread) {
		struct _MyThread *prev, *t = (struct _MyThread*) thread;
		if (t != NULL && t->parent != curr_thread)
				return -1;

		if (!is_child_active(curr_thread, t)) {
				//printf("Thread %d wants to wait for a child that is dead\n", curr_thread->thread_id);
				return 0;
		}

		//printf("Thread %d waiting on child %d\n", curr_thread->thread_id, t->thread_id);
		prev = curr_thread;
		prev->blocked_on_child = t->thread_id;
		enqueue(prev, &BlockL);
		curr_thread = dequeue(&ReadyQ);  /* TO DO: do we need to check if the ready queue is empty? 
										  * Will this condition exist? blocked queue non-empty and ready queue empty?
										  */
		if (curr_thread == NULL)
				setcontext(&init_context);

		//printf("Next thread to run is %d\n", curr_thread->thread_id);
		if (swapcontext(&prev->uctx, &curr_thread->uctx) == -1)
				handle_error("swapcontext failed");
		return 0;
}

/* Join with all children:
 * Joins the invoking function with all child threads,
 * i.e. block the invoking thread till all child threads have terminated.
 * Check the number of active child threads of the current thread context. 
 * Return immediately if there are no active children;
 * else add the current thread context to the blocked thread list,
 * and swap the current thread with the next available thread on the ready queue.
 *
 * Alternate implementation would be to call MyThreadJoin for all child threads,
 * but that could result in the frequent back-n-forth swapping of the invoking thread
 * from the ready queue to the blocked list,
 * as each call to MyThreadJoin can put the invoking thread on the blocked list.
 */
void MyThreadJoinAll(void) {
		struct _MyThread *prev;
		//printf("Thread %d wants to wait on all children\n", curr_thread->thread_id);
		if (curr_thread->no_of_active_children == 0) {
				//printf("all children already dead\n");
				return;
		}

		prev = curr_thread;
		prev->blocked_on_child = 0;
		enqueue(prev, &BlockL);
		curr_thread = dequeue(&ReadyQ);  /* TO DO: do we need to check if the ready queue is empty? 
										  * Will this condition exist? blocked queue non-empty and ready queue empty?
										  */

		if (curr_thread == NULL) 
				setcontext(&init_context);

		//printf("Next thread to run is %d\n", curr_thread->thread_id);
		if (swapcontext(&prev->uctx, &curr_thread->uctx) == -1)
				handle_error("swapcontext failed");

}

/* Terminate invoking thread:
 * Decrement the active child counter of the parent.
 * Remove the current thread from the children list of the parent.
 * Check if the parent thread needs to be unblocked.
 * The parent thread will need to be unblocked in 2 cases:
 * 1. The parent is specifically blocked on this particular child thread
 * 2. The parent is blocked on all child threads, but all other children have already terminated.
 * To unblock a parent, simply remove it from the blocked list 
 * and enqueue it to the ready queue.
 * Set parent pointer as NULL for all children of invoking thread.
 * Free all dynamic memory.
 * Set the first available thread from the ready queue for execution.
 *
 * All MyThreads are required to invoke this function, so they don't 
 * “fall out” of the start function.
 */
void MyThreadExit(void) {
		//printf("Thread %d exiting\n", curr_thread->thread_id);

		struct _MyThread *myparent = curr_thread->parent; 
		if (myparent != NULL) {
				myparent->no_of_active_children--;
				delete_child(myparent, curr_thread);		
				if ((myparent->blocked_on_child  == curr_thread->thread_id) ||
								(myparent->blocked_on_child == 0 && myparent->no_of_active_children == 0)) {
						delete_thread(myparent, &BlockL);     
						//printf("unblocked parent, adding to readyq\n");      
						enqueue(myparent, &ReadyQ);
						myparent->blocked_on_child = -1;
				}
		}

		update_parent_of_children(curr_thread);

		//if(curr_thread->uctx.uc_link != NULL) { 
		/* This will happen if the program contains explicit calls to MyThreadExit.
		 * we need to free the context structure set in the uc_link pointer
		 */
		free(curr_thread->uctx.uc_link->uc_stack.ss_sp);
		free(curr_thread->uctx.uc_link);
		//}

		free(curr_thread->uctx.uc_stack.ss_sp); 
		free(curr_thread);

		curr_thread = dequeue(&ReadyQ);  /* This should ideally never be NULL for the extra credit option,
										  * as main thread is always the last to execute,
										  * and main thread has a separate exit function.
										  */
		if (curr_thread == NULL) {
				//printf("Ready Q empty, going back to main\n");
				setcontext(&init_context);
		}

		//printf("Next thread to run is %d\n", curr_thread->thread_id);
		if (setcontext(&curr_thread->uctx) == -1)
				handle_error("setcontext failed");
}

// ****** CALLS ONLY FOR UNIX PROCESS ****** 
/* Create and run the "main" thread:
 * It is invoked only by the Unix process, and just once. 
 * The MyThread created is the oldest ancestor of all MyThreads
 * It returns when the thread ready queue is empty.
 */
void MyThreadInit(void(*start_funct)(void *), void *args) {
		MyThreadCreate(start_funct, args);

		curr_thread = dequeue(&ReadyQ);
		//printf("First thread to run with id %d\n", curr_thread->thread_id);

		if (swapcontext(&init_context, &curr_thread->uctx) == -1)
				handle_error("swapcontext failed");
}

/* Initialize the threads package and
 * converts the UNIX process context into a MyThread context.
 */
int MyThreadInitExtra(void) {
		curr_thread = make_new_thread();
		free(curr_thread->uctx.uc_link);
		/* TODO: We should make it point to a cleanup function that will release all dynamic memory
		 * associated with zombie child processes, and free up the ready and blocked queues.
		 * Somehow, changing the uc_link for the main thread is not working.
		 * Right now we are just letting the program terminate, and let the OS handle 
		 * reclaiming the memory.
		 */
		curr_thread->uctx.uc_link = NULL; 

		getcontext(&init_context); //not needed, just added as a precaution
		return 0;
}

/* Create a semaphore. Set the initial value to initialValue, which must be nonnegative.
 * A positive initial value has the same effect as invoking
 * MySemaphoreSignal the same number of times. On error it returns NULL.
 */
MySemaphore MySemaphoreInit(int initialValue) {
		struct _MySemaphore *s = malloc(sizeof(struct _MySemaphore));
		s->value = initialValue;
		s->BlockQ.head = s->BlockQ.tail = NULL;
		return (MySemaphore) s;
}

/* Signal a semaphore.
 * The invoking thread is not pre‐empted.
 */
void MySemaphoreSignal(MySemaphore sem) {
		struct _MySemaphore *s = sem;

		s->value++;
		if(s->value <= 0) { // when s->value is positive, its blocked queue is empty
				struct _MyThread *t = dequeue(&s->BlockQ);
				enqueue(t, &ReadyQ);
		} 
}

// Wait on a semaphore.
void MySemaphoreWait(MySemaphore sem) {
		struct _MySemaphore *s = sem;
		struct _MyThread *prev = curr_thread;

		s->value--;

		if(s->value < 0) {
				enqueue(curr_thread, &s->BlockQ);
				curr_thread = dequeue(&ReadyQ);

				if (curr_thread == NULL)
						setcontext(&init_context);

				//printf("Thread %d blocked on semaphore. Next thread to run is %d\n", prev->thread_id, curr_thread->thread_id);
				swapcontext(&(prev->uctx), &(curr_thread->uctx));
		}// else printf("Thread %d continuing, semaphore available\n", curr_thread->thread_id);
}

/* Destroy semaphore sem. 
 * Do not destroy semaphore if any threads are blocked on the queue. 
 * Return 0 on success, ‐1 on failure.
 */
int MySemaphoreDestroy(MySemaphore sem) {
		struct _MySemaphore *s = sem;

		if(s->value == 0) {
				free(s);
				return 0;
		}	
		return -1;
}
