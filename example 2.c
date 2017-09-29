#include <stdio.h>
#include "../mythread.h"

MySemaphore sem1, sem2;
void t4(void *d);

void t1(void *d) {
    printf("t1\n");
    MySemaphoreWait(sem1);
    printf("t1 locked sem1\n");
    MyThreadYield();
    MySemaphoreSignal(sem1);
    printf("t1 unlocked sem1\n");
    MyThreadCreate(t4, d);
    MyThreadJoinAll();
    MyThreadCreate(t4, d);
    MyThreadJoinAll();
    printf("t1 end\n");
	MyThreadExit();
}

void t2(void *d) {
    printf("t2 waiting for sem1\n"); 
    MySemaphoreWait(sem1);
    printf("t2 waiting for sem2\n"); 
    MySemaphoreWait(sem2);
    printf("t2 locked sem2\n");
    MySemaphoreSignal(sem1);
    printf("t2 unlocked sem1\n");
    MySemaphoreSignal(sem2);
    printf("t2 unlocked sem2\n");
    MyThread m = MyThreadCreate(t4, d);
    MyThreadJoin(m);
    printf("t2 end\n");
	MyThreadExit();
}

void t4(void *d) {
    printf("t4\n");
	MyThreadExit();
}

void t3(void *d) {
    printf("t3 Waiting for sem1\n");
    MySemaphoreWait(sem1);
    printf("t3 Waiting for sem2\n");
    MySemaphoreWait(sem2);
    MySemaphoreSignal(sem1);
    printf("t3 unlocked sem1\n");
    MySemaphoreSignal(sem2);
    printf("t3 unlocked sem2\n");
    printf("t3 end\n");
	MyThreadExit();
}

void t0(void * dummy)
{
  printf("t0 start\n");
  sem1 = MySemaphoreInit(1);
  sem2 = MySemaphoreInit(1);
  MyThreadCreate(t1, dummy);
  MyThreadCreate(t3, dummy);
  MyThreadCreate(t2, dummy);
  MyThreadJoinAll();
  MyThreadExit();
}

int main()
{
  MyThreadInit(t0, NULL);
  printf("Everything done!!\n");
}
