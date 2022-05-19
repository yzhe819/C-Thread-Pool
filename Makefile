test:
	gcc example.c thpool.c -pthread -o example.out
	./example.out

wait:
	gcc wait.c thpool.c -pthread -o wait.out
	./wait.out 2
	