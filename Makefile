compile:
	gcc main.c threadpool.c -pthread -o test.out

test:compile
	./test.out