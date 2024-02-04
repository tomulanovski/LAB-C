all: myshell mypipeline


myshell: myshell.o
	gcc -Wall -g -m32 myshell.o -o myshell

mypipeline: mypipeline.o
	gcc -Wall -g -m32 mypipeline.o -o mypipeline

mypipeline.o: mypipeline.c
	gcc -Wall -g -m32 -c mypipeline.c	

myshell.o: myshell.c
	gcc -Wall -g -m32 -c myshell.c

.PHONY: clean

clean :
	rm *.o myshell mypipeline
