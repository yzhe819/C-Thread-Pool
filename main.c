#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadPool.h"

void taskFunc(void* arg) {
    int num = *(int*)arg;
    printf("thread %ld is working, number is %d\n", pthread_self(), num);
    sleep(1);
}

int main() {
    // create thread pool
    ThreadPool* pool = threadPoolCreate(3, 4, 100);

    for (int i = 0; i < 100; i++) {
        // add task to the queue
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        threadPoolAdd(pool, taskFunc, num);
    }

    // wait for all tasks to be done
    sleep(30);
    printf("sleep 5 seconds...\n");

    threadPoolDestroy(pool);
    return 0;
}