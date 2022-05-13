#ifndef _THPOOL_
#define _THPOOL_

// The C implementation of a thread pool
#ifdef __cplusplus
extern "C" {
#endif

// define the thpool struct pointer type
typedef struct thpool_* threadpool;

// thread pool constructor
threadpool thpool_init(int num_threads);

// submit a job to the thread pool
int thpool_add_work(threadpool, void (*function_p)(void*), void* arg_p);

// wait for all jobs to finish
void thpool_wait(threadpool);

// start all threads in the thread pool
void thpool_resume(threadpool);

// stop the thread pool
void thpool_pause(threadpool);

// destroy the thread pool
void thpool_destroy(threadpool);

// get the number of threads in the pool
int thpool_num_threads_working(threadpool);

#ifdef __cplusplus
}
#endif

#endif /* _THPOOL_ */