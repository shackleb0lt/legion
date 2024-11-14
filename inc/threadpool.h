#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define THREAD_COUNT 16
#define TASK_QUEUE_SIZE 64

typedef void (*func_ptr_t)(void *arg);

typedef struct
{
    func_ptr_t func;
    void * arg;
} th_task;

typedef struct
{
    pthread_mutex_t qlock;
    pthread_cond_t task_avail;

    pthread_t thread_arr[THREAD_COUNT];

    th_task queue[TASK_QUEUE_SIZE];
    size_t queue_len;

    size_t first;
    size_t last;
    bool is_run;
} thpool_queue;

int init_threadpool();
void stop_threadpool();
int add_task_to_queue(func_ptr_t f_ptr, void * arg);

#endif