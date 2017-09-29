mythread.a: mythread.o utility.o
	ar rcs mythread.a mythread.o utility.o

mythread.o: mythread.c
	gcc -g -c mythread.c

utility.o: utility.c
	gcc -g -c utility.c
