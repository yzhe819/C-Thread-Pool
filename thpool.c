#include "thpool.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>  // for linux
#include <time.h>
#include <unistd.h>

// error messages
#define err(str) fprintf(stderr, str)

// global variables
static volatile int
    threads_keepalive;  // flag to check if all threads should live
static volatile int threads_on_hold;  // threads on hold by the thread manager

// structure
typedef struct bsem {       // binary semaphore
    pthread_mutex_t mutex;  // mutex lock
    pthread_cond_t cond;    // condition variable
    int v;
} bsem;

typedef struct job {              // the executed function
    struct job* prev;             // pointer to the prev job
    void (*function)(void* arg);  // function pointer
    void* arg;                    // function argument
} job;

typedef struct jobqueue {     // job queue
    pthread_mutex_t rwmutex;  // mutex lock for r/w operations
    bsem* has_jobs;  // binary semaphore for whether the queue is empty or not
    job* front;      // pointer to the front of the queue
    job* rear;       // pointer to the rear of the queue
    int len;         // length of the queue
} jobqueue;

typedef struct thread {
    int id;                    // thread id
    pthread_t pthread;         // pointer to the thread
    struct thpool_* thpool_p;  // belongs to this thread pool
} thread;

typedef struct thpool_ {
    thread** threads;                  // array of threads
    volatile int num_threads_alive;    // number of threads alive
    volatile int num_threads_working;  // number of threads working
    pthread_mutex_t thcount_lock;      // mutex lock for thread count
    pthread_cond_t threads_all_idle;  // condition variable for all threads idle
    jobqueue jobqueue;                // pointer to the job queue
} thpool_;

// function prototypes

// initialize a thread in the thread pool
static int thread_init(thpool_* thpool_p, struct thread** thread_p, int id);
// what the thread does
static void* thread_do(struct thread* thread_p);
// set the calling thread on hold
static void thread_hold(int sig_id);
// free a thread
static void thread_destroy(struct thread* thread_p);

// initialize the queue for jobs
static int jobqueue_init(jobqueue* jobqueue_p);
// clear the queue
static void jobqueue_clear(jobqueue* jobqueue_p);
// add a job to the queue
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob_p);
// take the first job from the queue (FIFO)
static struct job* jobqueue_pull(jobqueue* jobqueue_p);
// free the queue
static void jobqueue_destroy(jobqueue* jobqueue_p);

// initialize a binary semaphore with a default value 0/1
static void bsem_init(struct bsem* bsem_p, int value);
// reset a binary semaphore to 0
static void bsem_reset(struct bsem* bsem_p);
// post to at least one thread
static void bsem_post(struct bsem* bsem_p);
// post to all threads
static void bsem_post_all(struct bsem* bsem_p);
// wait on a binary semaphore until it is 0
static void bsem_wait(struct bsem* bsem_p);

// thread pool implementation

// initialize thread pool
struct thpool_* thpool_init(int num_threads) {
    threads_on_hold = 0;    // no thread on hold
    threads_keepalive = 1;  // all threads should be kept alive

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
    thpool_p->num_threads_alive = 0;    // no threads alive when pool created
    thpool_p->num_threads_working = 0;  // no threads working when pool created

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

// add work to the thread pool
int thpool_add_work(threadpool thpool_p,
                    void (*function_p)(void*),
                    void* arg_p) {
    job* newjob;

    newjob = (struct job*)malloc(sizeof(struct job));
    if (newjob == NULL) {
        err("thpool_add_work(): Could not allocate memory for new job");
        return -1;
    }

    newjob->function = function_p;
    newjob->arg = arg_p;

    // add the job to the queue
    jobqueue_push(&thpool_p->jobqueue, newjob);

    return 0;
}

// wait for all threads in thread pool to finish
void thpool_wait(thpool_* thpool_p) {
    pthread_mutex_lock(&thpool_p->thcount_lock);
    // wait while jobqueue is not empty or while threads are still working
    while (thpool_p->jobqueue.len || thpool_p->num_threads_working) {
        pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
    }
    pthread_mutex_unlock(&thpool_p->thcount_lock);
}

// destroy the thread pool
void thpool_destroy(thpool_* thpool_p) {
    // no need to destroy if it was never initialized
    if (thpool_p == NULL) {
        return;
    }

    // find out all the number of active threads
    volatile int threads_total = thpool_p->num_threads_alive;
    threads_keepalive = 0;

    // give one second to kill idle threads
    double TIMEOUT = 1.0;
    time_t start, end;
    double tpassed = 0.0;
    time(&start);
    // loop while there are still active threads or if timeout is not reached
    while (tpassed < TIMEOUT && thpool_p->num_threads_alive) {
        bsem_post_all(thpool_p->jobqueue.has_jobs);
        time(&end);
        tpassed = difftime(end, start);
    }

    // the timeout is reached, due to their still have work to do
    // poll the remaining threads
    while (thpool_p->num_threads_alive) {
        bsem_post_all(thpool_p->jobqueue.has_jobs);
        sleep(1);  // wait for one second
    }

    // destroy the job queue
    jobqueue_destroy(&thpool_p->jobqueue);

    // free all the threads
    for (int n = 0; n < threads_total; n++) {
        thread_destroy(thpool_p->threads[n]);
    }

    // free thread pool
    free(thpool_p->threads);
    free(thpool_p);
}

// pause all the threads in the thread pool
void thpool_pause(thpool_* thpool_p) {
    for (int n = 0; n < thpool_p->num_threads_alive; n++) {
        // using pthread_kill to send the signal to a specific thread
        // SIGUSR1: intended for use by user applications.
        // https://www.ibm.com/docs/en/zos/2.3.0?topic=functions-sigaction-examine-change-signal-action#rtsigac__trs1
        pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
    }
}

// resume all the threads in the thread pool
void thpool_resume(thpool_* thpool_p) {
    threads_on_hold = 0;
}

// get the number of working threads in the thread pool
int thpool_num_threads_working(thpool_* thpool_p) {
    return thpool_p->num_threads_working;
}

// thread implementation
static int thread_init(thpool_* thpool_p, struct thread** thread_p, int id) {
    // allocate memory for thread structure
    *thread_p = (struct thread*)malloc(sizeof(struct thread));
    if (*thread_p == NULL) {
        err("thread_init(): Could not allocate memory for thread");
        return -1;
    }

    // init the thread id and related thread pool
    (*thread_p)->id = id;
    (*thread_p)->thpool_p = thpool_p;

    // create a new thread
    pthread_create(&(*thread_p)->pthread, NULL, (void* (*)(void*))thread_do,
                   *thread_p);
    // pthread_detach - detach a thread from the calling process
    pthread_detach((*thread_p)->pthread);
    return 0;
}

static void thread_hold(int sig_id) {
    // from the source code
    // the feature of act.sa_handler, which will transfer the signal id to this
    // function, then it can handle different logics
    (void)sig_id;
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
    // The sigaction() system call is used to change the action taken by
    // a process on receipt of a specific signal.
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    // specifies a set of flags which modify the behavior of the signal.
    act.sa_flags = 0;
    // specifies the action to be associated with signum
    act.sa_handler = thread_hold;
    // sign the SIGUSRS1 signal
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        err("thread_do(): Could not set signal handler");
        return NULL;
    }

    // mark thread as alive
    pthread_mutex_lock(&thpool_p->thcount_lock);
    thpool_p->num_threads_alive++;
    pthread_mutex_unlock(&thpool_p->thcount_lock);

    // the global value control the thread finish or running
    while (threads_keepalive) {
        // wait for work to do
        bsem_wait(thpool_p->jobqueue.has_jobs);

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
                func_buff = job_p->function;
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

    // mark this thread is not alive
    pthread_mutex_lock(&thpool_p->thcount_lock);
    thpool_p->num_threads_alive--;
    pthread_mutex_unlock(&thpool_p->thcount_lock);

    return NULL;
}

static void thread_destroy(thread* thread_p) {
    // free thread resources
    free(thread_p);
}

// job queue implementation
static int jobqueue_init(jobqueue* jobqueue_p) {
    // init the job queue
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    jobqueue_p->len = 0;

    jobqueue_p->has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
    if (jobqueue_p->has_jobs == NULL) {
        return -1;
    }

    // init the read/write operation mutex
    pthread_mutex_init(&jobqueue_p->rwmutex, NULL);
    // init the binary semaphore
    bsem_init(jobqueue_p->has_jobs, 0);

    return 0;
}

static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob) {
    // lock this job queue
    pthread_mutex_lock(&jobqueue_p->rwmutex);
    newjob->prev = NULL;

    // check the len of job queue
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
    // unlock the job queue
    pthread_mutex_unlock(&jobqueue_p->rwmutex);
}

static void jobqueue_clear(jobqueue* jobqueue_p) {
    // pull all the jobs and free them
    while (jobqueue_p->len) {
        free(jobqueue_pull(jobqueue_p));
    }
    // release the resource
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    bsem_reset(jobqueue_p->has_jobs);
    jobqueue_p->len = 0;
}

static struct job* jobqueue_pull(jobqueue* jobqueue_p) {
    // lock this job queue
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
    // clear the job queue and then destroy it
    jobqueue_clear(jobqueue_p);
    free(jobqueue_p->has_jobs);
}

// bsem implementation
static void bsem_init(bsem* bsem_p, int value) {
    // invalid value
    if (value < 0 || value > 1) {
        err("bsem_init(): Binary semaphore can take only values 1 or 0");
        exit(1);
    }
    // init the mutex and condition variable
    pthread_mutex_init(&bsem_p->mutex, NULL);
    pthread_cond_init(&bsem_p->cond, NULL);
    bsem_p->v = value;
}

static void bsem_reset(bsem* bsem_p) {
    // reset the binary semaphore to the initial value
    bsem_init(bsem_p, 0);
}

static void bsem_post(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    // signal the condition variable (unblock one thread)
    pthread_cond_signal(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

static void bsem_post_all(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    // unblock threads blocked on a condition variable
    pthread_cond_broadcast(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

static void bsem_wait(bsem* bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    // while the value is 0, block the thread on the condition variable
    while (bsem_p->v != 1) {
        pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
    }
    // reset back to 0
    bsem_p->v = 0;
    pthread_mutex_unlock(&bsem_p->mutex);
}