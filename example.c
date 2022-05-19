#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thpool.h"

void task(void* arg) {
    printf("Thread #%u working on task #%d\n", (int)pthread_self(), *(int*)arg);
    sleep(2);
}

int main() {
    // create thread pool
    threadpool thpool = thpool_init(2);

    // check the task pool
    puts("Adding 40 tasks to threadpool");
    for (int i = 0; i < 40; i++) {
        int* num = (int*)malloc(sizeof(int));
        *num = i;
        thpool_add_work(thpool, task, num);
    };

    // wait for all tasks to complete
    thpool_wait(thpool);
    puts("Killing threadpool");
    thpool_destroy(thpool);

    return 0;
}
