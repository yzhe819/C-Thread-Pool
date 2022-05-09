#ifndef ThreadPool_h
#define ThreadPool_h

#include <pthread.h>
#include "./DataStructures/Queue.h"

struct ThreadJob {
    void* (*job)(void* arg);
    void* arg;
};

struct ThreadPool {
    int num_threads;
    int active;
    int workCount;
    struct Queue work;
    pthread_t* pool;
    pthread_mutex_t lock;
    pthread_cond_t signal;

    void (*add_work)(struct ThreadPool* thread_pool,
                     struct ThreadJob thread_job);
};

struct ThreadPool thread_pool_constructor(int num_threads);
struct ThreadJob thread_job_constructor(void* (*func)(void* arg), void* arg);

void thread_pool_destructor(struct ThreadPool* thread_pool);

#endif /* ThreadPool_h */