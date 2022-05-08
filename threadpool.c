#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int NUMBER = 2;

// struct of a task
typedef struct Task {
    void (*function)(void* arg);  // function to execute
    void* arg;                    // argument to pass to function
} Task;

// struct of thread pool
struct ThreadPool {
    Task* queue;        // task queue
    int queueCapacity;  // capacity of queue
    int queueSize;      // number of tasks in queue
    int queueFront;     // index of front task
    int queueRear;      // index of rear task

    pthread_t managerID;        // thread ID of manager
    pthread_t* threadIDs;       // thread ID of workers
    int minNum;                 // minimum number of threads
    int maxNum;                 // maximum number of threads
    int busyNum;                // number of current threads
    int liveNum;                // number of existing threads
    int exitNum;                // number of threads needed to destroy
    pthread_mutex_t mutexpool;  // mutex for thread pool
    pthread_mutex_t mutexBusy;  // mutex for busy number
    pthread_cond_t notFull;     // condition variable for full queue
    pthread_cond_t notEmpty;    // condition variable for empty queue

    int shutdown;  // flag to shutdown thread pool
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));

    do {
        if (pool == NULL) {
            printf("malloc threadpool failed...\n");
            break;
        }

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL) {
            printf("malloc threadIDs failed...\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);

        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;  // start with min number of threads
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0 ||
            pthread_cond_init(&pool->notEmpty, NULL) != 0) {
            printf("init mutex or cond failed...\n");
            break;
        }

        // task queue
        pool->queue = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        // create manager thread
        pthread_create(&pool->managerID, NULL, manager, pool);

        // create worker threads
        for (int i = 0; i < min; i++) {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }

        return pool;
    } while (0);

    // free resource if failed
    if (pool->threadIDs) {
        free(pool->threadIDs);
    }
    if (pool->queue) {
        free(pool->queue);
    }
    if (pool) {
        free(pool);
    }

    return NULL;
}

void* worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutexpool);
        // check if thread pool is shutdown and have no task
        while (pool->queueSize == 0 && !pool->shutdown) {
            // worker is idle and wait for task
            pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

            // check whether the thread should be destroyed
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    // unlock mutex before destroy
                    pthread_mutex_unlock(&pool->mutexpool);
                    threadExit(pool);
                }
            }
        }

        // check if thread pool is shutdown
        if (pool->shutdown) {
            // avoid deadlock
            pthread_mutex_unlock(&pool->mutexpool);
            threadExit(pool);
        }

        // get task from queue
        Task task;
        task.function = pool->queue[pool->queueFront].function;
        task.arg = pool->queue[pool->queueFront].arg;

        // move queue front, circular queue
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        // unlock the manager thread
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexpool);

        // execute task
        printf("worker %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);  // or (*task.function)(task.arg);
        free(task.arg);
        task.arg = NULL;

        printf("worker %ld end working ...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while (!pool->shutdown) {
        // frequently check the thread pool status
        sleep(3);  // each 3 seconds

        // get the task and current thread number in thread pool
        pthread_mutex_lock(&pool->mutexpool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexpool);

        // get the busy number from thread pool
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // add new threads if necessary
        // if task number is larger than existing thread and existing thread
        // number is smaller than the max thread number, add new threads
        if (queueSize > liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexpool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER &&
                            pool->liveNum < pool->maxNum;
                 i++) {
                if (pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexpool);
        }

        // destroy threads if necessary
        // if the busy thread times two but it is still smaller than the
        // existing thread number and the existing thread number is larger than
        // the min thread number, destroy threads
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexpool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexpool);

            // notify the threads to exit
            for (int i = 0; i < NUMBER; i++) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool) {
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; i++) {
        if (pool->threadIDs[i] == tid) {
            printf("thread %ld is exiting...\n", tid);
            pool->threadIDs[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool* pool, void (*func)(void*), void* arg) {
    pthread_mutex_lock(&pool->mutexpool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
        // stop the worker threads
        pthread_cond_wait(&pool->notFull, &pool->mutexpool);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutexpool);
        return;
    }

    // add task to queue
    pool->queue[pool->queueRear].function = func;
    pool->queue[pool->queueRear].arg = arg;

    // move queue rear, circular queue
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    // notify the worker thread
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexpool);
}

int threadPoolBusyNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexpool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexpool);
    return liveNum;
}

int threadPoolDestroy(ThreadPool* pool) {
    if (pool == NULL) {
        return -1;
    }

    // set shutdown flag
    pool->shutdown = 1;
    // recycling manager thread
    pthread_join(pool->managerID, NULL);
    // notify the worker threads to exit
    for (int i = 0; i < pool->liveNum; i++) {
        pthread_cond_signal(&pool->notEmpty);
    }

    // free memory
    if (pool->queue) {
        free(pool->queue);
    }
    if (pool->threadIDs) {
        free(pool->threadIDs);
    }

    // destroy mutex and condition
    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}
