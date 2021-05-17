CFLAGS=-c -std=gnu99 -Wall -pedantic -I$(INCLUDE) -g

main: main.o
	gcc -o main main.o -pthread

main.o: main.c
		gcc $(CFLAGS) main.c -o main.o

clean:
	rm -f main.o main
