all: findeq.c findeq.o
	gcc -c findeq.c -o findeq.o
	gcc findeq.o -o findeq
clean:
	rm -rf findeq findeq.o
