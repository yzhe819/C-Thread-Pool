dev:compile
	./ThreadPool

test:
	gcc test.c LinkedList.c Node.c -o test
	./test

compile:
	gcc ThreadPool.c -o ThreadPool