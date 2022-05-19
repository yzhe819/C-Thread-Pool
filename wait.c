#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "thpool.h"

void sleep_4_secs() {
    sleep(4);
    puts("SLEPT");
}

int main(int argc, char* argv[]) {
    char* p;
    if (argc != 2) {
        puts("This testfile needs excactly one arguments");
        exit(1);
    }
    int num_threads = strtol(argv[1], &p, 10);

    threadpool thpool = thpool_init(num_threads);

    thpool_pause(thpool);

    // Since pool is paused, threads should not start before main's sleep
    thpool_add_work(thpool, (void*)sleep_4_secs, NULL);
    thpool_add_work(thpool, (void*)sleep_4_secs, NULL);

    puts("The main function will sleep for 3 seconds");
    sleep(3);

    // Now we will start threads in no-parallel with main
    puts("Starting threads");
    thpool_resume(thpool);

    puts("The main function will sleep for 2 seconds");
    sleep(2);  // Give some time to threads to get the work

    puts("Wait for work to finish");
    thpool_destroy(thpool);  // Wait for work to finish

    return 0;
}