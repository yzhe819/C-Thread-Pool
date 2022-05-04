# dev:compile
# 	./ThreadPool

test_linkedlist:
	gcc Linkedlist.test.c LinkedList.c Node.c -o test
	./test

test_queue:
	gcc Queue.test.c LinkedList.c Node.c Queue.c -o test
	./test

# compile:
# 	gcc ThreadPool.c -o ThreadPool