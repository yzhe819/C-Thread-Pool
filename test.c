#include <stdio.h>
#include "ThreadPool.h"

void* sum(void* a) {
    // should just print the hello world
    printf("hello world\n");
}

int main() {
    struct ThreadPool thread_pool = thread_pool_constructor(1);

    for (int i = 0; i < 10; i++) {
        struct ThreadJob job = thread_job_constructor(sum, &i);
        thread_pool.add_work(&thread_pool, job);
    }

    while (1) {
        // used for check the output of thread pool
    }
    return 0;
}