#include "ThreadPool.h"
#include <stdio.h>
#include <stdlib.h>

void* thread_job(void* arg);
void add_work(struct ThreadPool* thread_pool, struct ThreadJob job);

struct ThreadPool thread_pool_constructor(int num_threads) {
    struct ThreadPool thread_pool;
    thread_pool.num_threads = num_threads;
    thread_pool.active = 1;
    thread_pool.work = queue_constructor();
    thread_pool.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    thread_pool.signal = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    pthread_mutex_lock(&thread_pool.lock);
    thread_pool.pool = malloc(sizeof(pthread_t[num_threads]));
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&thread_pool.pool[i], NULL, thread_job, &thread_pool);
    }
    pthread_mutex_unlock(&thread_pool.lock);
    thread_pool.add_work = add_work;
    return thread_pool;
}

void thread_pool_destructor(struct ThreadPool* thread_pool) {
    thread_pool->active = 0;
    // finish the thread function first
    for (int i = 0; i < thread_pool->num_threads; i++) {
        pthread_cond_signal(&thread_pool->signal);
    }
    // join the finished threads
    for (int i = 0; i < thread_pool->num_threads; i++) {
        pthread_join(thread_pool->pool[i], NULL);
    }
    free(thread_pool->pool);
    queue_destructor(&thread_pool->work);
}

struct ThreadJob thread_job_constructor(void* (*func)(void* args), void* args) {
    struct ThreadJob job;
    job.job = func;
    job.args = args;
    return job;
}

void* thread_job(void* args) {
    struct ThreadPool* thread_pool = (struct ThreadPool*)args;
    while (thread_pool->active == 1) {
        pthread_mutex_lock(&thread_pool->lock);
        pthread_cond_wait(&thread_pool->signal, &thread_pool->lock);
        struct ThreadJob job =
            *(struct ThreadJob*)thread_pool->work.peek(&thread_pool->work);

        thread_pool->work.pop(&thread_pool->work);
        pthread_mutex_unlock(&thread_pool->lock);
        if (job.job) {
            job.job(job.args);
        }
    }
    return NULL;
}

void add_work(struct ThreadPool* thread_pool, struct ThreadJob job) {
    pthread_mutex_lock(&thread_pool->lock);
    // todo update the node section
    thread_pool->work.push(&thread_pool->work, &job, Special, sizeof(job));
    pthread_cond_signal(&thread_pool->signal);
    pthread_mutex_unlock(&thread_pool->lock);
}