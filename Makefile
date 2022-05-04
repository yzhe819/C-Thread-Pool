# dev:compile
# 	./ThreadPool

test_linkedlist:
	gcc ./Tests/Linkedlist.test.c ./DataStructures/LinkedList.c ./DataStructures/Node.h -o test
	./test

test_queue:
	gcc ./Tests/Queue.test.c ./DataStructures/LinkedList.c ./DataStructures/Node.h ./DataStructures/Queue.c -o test
	./test

# compile:
# 	gcc ThreadPool.c -o ThreadPool