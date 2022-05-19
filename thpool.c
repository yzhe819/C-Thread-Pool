#include "thpool.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>  // for linux
#include <time.h>
#include <unistd.h>

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
static void* thread_do(struct thread* thread_p);
static void thread_hold(int sig_id);
static void thread_destroy(struct thread* thread_p);

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
    for (int n = 0; n < num_threads; n++) {
        thread_init(thpool_p, &thpool_p->threads[n], n);
    }

    // wait for threads to initialize
    while (thpool_p->num_threads_alive != num_threads) {
    }

    return thpool_p;
}

int thpool_add_work(threadpool thpool_p,
                    void (*function_p)(void*),
                    void* arg_p) {
    job* newjob;

    newjob = (struct job*)malloc(sizeof(struct job));
    if (newjob == NULL) {
        err("thpool_add_work(): Could not allocate memory for new job");
        return -1;
    }

    newjob->function_p = function_p;
    newjob->arg = arg_p;

    jobqueue_push(&thpool_p->jobqueue, newjob);

    return 0;
}

void thpool_wait(thpool_* thpool_p) {
    pthread_mutex_lock(&thpool_p->thcount_lock);
    while (thpool_p->jobqueue.len || thpool_p->num_threads_working) {
        pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
    }
    pthread_mutex_unlock(&thpool_p->thcount_lock);
}

void thpool_destroy(thpool_* thpool_p) {
    if (thpool_p == NULL) {
        return;
    }

    volatile int threads_total = thpool_p->num_threads_alive;
    threads_keepalive = 0;

    // give one second to kill idle threads
    double TIMEOUT = 1.0;
    time_t start, end;
    double tpassed = 0.0;
    time(&start);
    while (tpassed < TIMEOUT && thpool_p->num_threads_alive) {
        bsem_post_all(thpool_p->jobqueue.has_jobs);
        time(&end);
        tpassed = difftime(end, start);
    }

    // poll the remaining threads
    while (thpool_p->num_threads_alive) {
        bsem_post_all(thpool_p->jobqueue.has_jobs);
        sleep(1);
    }

    // destroy the job queue
    jobqueue_destroy(&thpool_p->jobqueue);

    // free all the threads
    for (int n = 0; n < threads_total; n++) {
        thread_destroy(thpool_p->threads[n]);
    }

    free(thpool_p->threads);
    free(thpool_p);
}

void thpool_pause(thpool_* thpool_p) {
    for (int n = 0; n < thpool_p->num_threads_alive; n++) {
        pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
    }
}

void thpool_resume(thpool_* thpool_p) {
    // this will stop all the thread pools
    threads_on_hold = 0;
}

int thpool_num_threads_working(thpool_* thpool_p) {
    return thpool_p->num_threads_working;
}

// thread implementation
static int thread_init(thpool_* thpool_p, struct thread** thread_p, int id) {
    *thread_p = (struct thread*)malloc(sizeof(struct thread));
    if (*thread_p == NULL) {
        err("thread_init(): Could not allocate memory for thread");
        return -1;
    }

    (*thread_p)->id = id;
    (*thread_p)->thpool_p = thpool_p;

    pthread_create(&(*thread_p)->pthread, NULL, (void* (*)(void*))thread_do,
                   *thread_p);
    pthread_detach((*thread_p)->pthread);
    return 0;
}

static void thread_hold(int sig_id) {
    (void)sig_id;  //???
    threads_on_hold = 1;
    while (threads_on_hold) {
        sleep(1);
    }
}

static void* thread_do(struct thread* thread_p) {
    // set the thread name
    char thread_name[32] = {0};
    sprintf(thread_name, "thread-%d", thread_p->id);
    prctl(PR_SET_NAME, thread_name);  // only for linux

    thpool_* thpool_p = thread_p->thpool_p;

    // register signal handler
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = thread_hold;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        err("thread_do(): Could not set signal handler");
        return NULL;
    }

    // mark thread as alive
    pthread_mutex_lock(&thpool_p->thcount_lock);
    thpool_p->num_threads_alive++;
    pthread_mutex_unlock(&thpool_p->thcount_lock);

    while (threads_keepalive) {
        // wait for work to do
        bsem_wait(thpool_p->jobqueue->has_jobs);

        // exit thread if all jobs are done
        if (threads_keepalive) {
            // mark thread as working
            pthread_mutex_lock(&thpool_p->thcount_lock);
            thpool_p->num_threads_working++;
            pthread_mutex_unlock(&thpool_p->thcount_lock);

            void (*func_buff)(void*);
            void* arg_buff;

            // get job from queue
            job* job_p = jobqueue_pull(&thpool_p->jobqueue);
            if (job_p) {
                func_buff = job_p->function_p;
                arg_buff = job_p->arg;
                // execute the job
                func_buff(arg_buff);
                free(job_p);
            }

            pthread_mutex_lock(&thpool_p->thcount_lock);
            thpool_p->num_threads_working--;
            // no running threads
            if (!thpool_p->num_threads_working) {
                pthread_cond_signal(&thpool_p->threads_all_idle);
            }
            pthread_mutex_unlock(&thpool_p->thcount_lock);
        }
    }

    // remove the alive thread count
    pthread_mutex_lock(&thpool_p->thcount_lock);
    thpool_p->num_threads_alive--;
    pthread_mutex_unlock(&thpool_p->thcount_lock);

    return NULL;
}

static void thread_destroy(thread* thread_p) {
    free(thread_p);
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

// bsem implementation
static void bsem_init(bsem* bsem_p, int value) {
    if (value < 0 || value > 1) {
        err("bsem_init(): Binary semaphore can take only values 1 or 0");
        exit(1);
    }
    pthread_mutex_init(&bsem_p->mutex, NULL);
    pthread_cond_init(&bsem_p->cond, NULL);
    bsem_p->v = value;
}

static void bsem_reset(bsem* bsem_p) {
    bsem_init(bsem_p, 0);
}

static void bsem_post(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    pthread_cond_signal(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

static void bsem_post_all(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    pthread_cond_broadcast(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

static void bsem_wait(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    while (bsem_p->v != 1) {
        pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
    }
    bsem_p->v = 0;
    pthread_mutex_unlock(&bsem_p->mutex);
}