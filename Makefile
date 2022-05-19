test: compile
	./example.out

compile:
	gcc example.c thpool.c -pthread -o example.out