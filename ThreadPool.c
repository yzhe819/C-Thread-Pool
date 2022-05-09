#include "ThreadPool.h"
#include <stdio.h>
#include <stdlib.h>

void* thread_job(void* arg);
void add_work(struct ThreadPool* thread_pool, struct ThreadJob thread_job);

struct ThreadPool thread_pool_constructor(int num_threads) {
    struct ThreadPool thread_pool;
    thread_pool.num_threads = num_threads;
    thread_pool.active = 1;
    thread_pool.workCount = 0;
    thread_pool.work = queue_constructor();
    pthread_mutex_init(&thread_pool.lock, NULL);
    pthread_cond_init(&thread_pool.signal, NULL);
    thread_pool.add_work = add_work;
    // pthread_mutex_lock(&thread_pool.lock);
    thread_pool.pool = malloc(sizeof(pthread_t[num_threads]));
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&thread_pool.pool[i], NULL, thread_job, &thread_pool);
    }
    // pthread_mutex_unlock(&thread_pool.lock);
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

struct ThreadJob thread_job_constructor(void* (*func)(void* arg), void* arg) {
    struct ThreadJob job;
    job.job = func;
    job.arg = arg;
    return job;
}

void* thread_job(void* arg) {
    struct ThreadPool* thread_pool = (struct ThreadPool*)arg;
    while (1) {
        printf("thread %ld is working\n", pthread_self());
        pthread_mutex_lock(&thread_pool->lock);
        while (thread_pool->workCount == 0) {
            pthread_cond_wait(&thread_pool->signal, &thread_pool->lock);
        }
        printf("thread %ld get job\n", pthread_self());
        struct ThreadJob job =
            *(struct ThreadJob*)thread_pool->work.peek(&thread_pool->work);
        thread_pool->work.pop(&thread_pool->work);
        thread_pool->workCount--;
        pthread_mutex_unlock(&thread_pool->lock);
        if (job.job) {
            job.job(job.arg);
        }
    }
    printf("thread %ld exit\n", pthread_self());
    return NULL;
}

void add_work(struct ThreadPool* thread_pool, struct ThreadJob thread_job) {
    pthread_mutex_lock(&thread_pool->lock);
    thread_pool->workCount++;
    // todo update the node section
    printf("submit work into queue\n");
    thread_pool->work.push(&thread_pool->work, &thread_job, Special,
                           sizeof(thread_job));
    pthread_mutex_unlock(&thread_pool->lock);
    pthread_cond_signal(&thread_pool->signal);
    printf("send the condition signal\n");
}