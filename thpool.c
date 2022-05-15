#include "thpool.h"
#include <errno.h>
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
    pthread_mutex_t thcount_lock;      // mutex lock for thread count
    pthread_cond_t threads_all_idle;  // condition variable for all threads idle
    jobqueue* jobqueue;               // pointer to the job queue
} thpool_;

// function prototypes
static int thread_init(thpool_* thpool_p, struct thread** thread_p, int id);
static void* thread_do(void* thread_p);
static void thread_hold(int sig_id);
static void thread_destory(struct thread* thread_p);

static int jobqueue_init(jobqueue* jobqueue_p);
static void jobqueue_clear(jobqueue* jobqueue_p);
static void jobqueue_push(jobqueue* jobqueue_p, job* newjob_p);
static struct job* jobqueue_pull(jobqueue* jobqueue_p);
static void jobqueue_destroy(jobqueue* jobqueue_p);

static void bsem_init(struct bsem* bsem_p, int value);
static void bsem_reset(struct bsem* bsem_p);
static void bsem_post(struct bsem* bsem_p);
static void bsem_post_all(struct bsem* bsem_p);
static void bsem_wait(struct bsem* bsem_p);

// threadpool implementation
struct thpool_* thpool_init(int num_threads) {
    threads_on_hold = 0;
    threads_keepalive = 1;

    if (num_threads < 0) {
        num_threads = 0;
    }

    // make new thread pool
    thpool_* thpool_p;
    thpool_p = (struct thpool_*)malloc(sizeof(struct thpool_));
    if (thpool_p == NULL) {
        err("thpool_init(): Could not allocate memory for thread pool");
        return NULL;
    }
    thpool_p->num_threads_alive = 0;
    thpool_p->num_threads_working = 0;

    // init the job queue
    if (jobqueue_init(&thpool_p->jobqueue) == -1) {
        err("thpool_init(): Could not allocate memory for job queue\n");
        free(thpool_p);
        return NULL;
    }

    // init the threads in thread pool
    thpool_p->threads =
        (struct thread**)malloc(num_threads * sizeof(struct thread*));
    if (thpool_p->threads == NULL) {
        err("thpool_init(): Could not allocate memory for threads");
        jobqueue_destroy(&thpool_p->jobqueue);
        free(thpool_p);
        return NULL;
    }

    // init the mutex and conditional variable for thread count
    pthread_mutex_init(&thpool_p->thcount_lock, NULL);
    pthread_cond_init(&thpool_p->threads_all_idle, NULL);

    // thread init
    for (int n = 0; i < num_threads; n++) {
        thread_init(thpool_p, &thpool_p->threads[n], n);
    }

    // wait for threads to initialize
    while (thpool_p->num_threads_alive != num_threads) {
    }

    return thpool_p;
}

// job queue implementation
static int jobqueue_init(jobqueue* jobqueue_p) {
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    jobqueue_p->len = 0;

    jobqueue_p->has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
    if (jobqueue_p->has_jobs == NULL) {
        return -1;
    }

    pthread_mutex_init(&jobqueue_p->rwmutex, NULL);
    bsem_init(jobqueue_p->has_jobs, 0);

    return 0;
}

static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob) {
    pthread_mutex_lock(&jobqueue_p->rwmutex);
    newjob->prev = NULL;

    switch (jobqueue_p->len) {
        case 0:  // no jobs in queue
            jobqueue_p->front = newjob;
            jobqueue_p->rear = newjob;
            break;
        default:  // one or more jobs in queue
            jobqueue_p->rear->prev = newjob;
            jobqueue_p->rear = newjob;
    }
    jobqueue_p->len++;

    bsem_post(jobqueue_p->has_jobs);
    pthread_mutex_unlock(&jobqueue_p->rwmutex);
}

static void jobqueue_clear(jobqueue* jobqueue_p) {
    while (jobqueue_p->len) {
        free(jobqueue_pull(jobqueue_p));
    }
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    bsem_reset(jobqueue_p->has_jobs);
    jobqueue_p->len = 0;
}

static struct job* jobqueue_pull(jobqueue* jobqueue_p) {
    pthread_mutex_lock(&jobqueue_p->rwmutex);
    job* job_p = jobqueue_p->front;

    switch (jobqueue_p->len) {
        case 0:  // no jobs in queue
            break;
        case 1:  // only one job in queue
            jobqueue_p->front = NULL;
            jobqueue_p->rear = NULL;
            jobqueue_p->len = 0;
            break;
        default:  // more than one job in queue
            jobqueue_p->front = job_p->prev;
            jobqueue_p->len--;
            // more than one job in queue -> post it
            bsem_post(jobqueue_p->has_jobs);
    }

    pthread_mutex_unlock(&jobqueue_p->rwmutex);
    return job_p;
}

static void jobqueue_destroy(jobqueue* jobqueue_p) {
    jobqueue_clear(jobqueue_p);
    free(jobqueue_p->has_jobs);
}