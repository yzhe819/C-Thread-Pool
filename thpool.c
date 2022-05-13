#include "thpool.h"
#include <pthread.h>

// global variables
static volatile int threads_keepalive;
static volatile int threads_on_hold;

// structure
typedef struct bsem {       // binary semaphore
    pthread_mutex_t mutex;  // mutex lock
    pthread_cond_t cond;    // condition variable
    int v;
} bsem;

typedef struct job {                // the executed function
    struct job* prev;               // pointer to the prev job
    void (*function_p)(void* arg);  // function pointer
    void* arg;                      // function argument
} job;

typedef struct jobqueue {
    pthread_mutex_t rwmutex;  // mutex lock for w/r operations
    bsem* has_jobs;  // binary semaphore for whether the queue is empty or not
    job* front;      // pointer to the front of the queue
    job* rear;       // pointer to the rear of the queue
    int len;         // length of the queue
} jobqueue;

typedef struct thread {
    int id;                    // thread id
    pthread_t pthread;         // pointer to the thread
    struct thpool_* thpool_p;  // access to the threadpool
} thread;

typedef struct thpool_ {
    thread** threads;                  // array of threads
    volatile int num_threads_alive;    // number of threads alive
    volatile int num_threads_working;  // number of threads working
    pthread_mutex_t thcount_mutex;     // mutex lock for thread count
    pthread_cond_t threads_all_idle;  // condition variable for all threads idle
    jobqueue* jobqueue;               // pointer to the job queue
} thpool_;
