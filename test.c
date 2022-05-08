#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ThreadPool.h"

void* taskFunc(void* arg) {
    int num = *(int*)arg;
    printf("thread %ld is working, number is %d\n", pthread_self(), num);
    sleep(1);
}

int main() {
    struct ThreadPool thread_pool = thread_pool_constructor(8);

    for (int i = 0; i < 10; i++) {
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        struct ThreadJob job = thread_job_constructor(taskFunc, num);
        thread_pool.add_work(&thread_pool, job);
    }

    printf("sleep 30 seconds...\n");
    sleep(30);
    return 0;
}