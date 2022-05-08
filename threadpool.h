#ifndef _THREADPOOL_H
#define _THREADPOOL_H

typedef struct ThreadPool ThreadPool;

// constructor
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

// destructor
int threadPoolDestroy(ThreadPool* pool);

// add task to the queue
void threadPoolAdd(ThreadPool* pool, void (*func)(void*), void* arg);

// get the number of working threads in the pool
int threadPoolBusyNum(ThreadPool* pool);

// get the number of existing threads in the pool
int threadPoolAliveNum(ThreadPool* pool);

void* worker(void* arg);            // worker thread
void* manager(void* arg);           // manager thread
void threadExit(ThreadPool* pool);  // exit the thread

#endif  // _THREADPOOL_H