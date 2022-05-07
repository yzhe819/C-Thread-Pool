# dev:compile
# 	./ThreadPool

test_linkedlist:
	mkdir -p bin
	gcc ./Tests/Linkedlist.test.c ./DataStructures/LinkedList.c ./DataStructures/Node.c -o ./bin/test
	./bin/test

test_queue:
	mkdir -p bin
	gcc ./Tests/Queue.test.c ./DataStructures/LinkedList.c ./DataStructures/Node.c ./DataStructures/Queue.c -o ./bin/test
	./bin/test

test_threadpool:
	gcc -pthread ./test.c ./ThreadPool.c ./DataStructures/LinkedList.c ./DataStructures/Node.c ./DataStructures/Queue.c -o ./bin/test
	./bin/test