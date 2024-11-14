#include "threadpool.h"

thpool_queue g_th_queue;

/**
 * Function to add task to the queue,
 * If the queue is full - 1 is returned
 * Check threadpool.h for queue size limits
 */
int add_task_to_queue(func_ptr_t f_ptr, void * arg)
{
    int next = 0;

    if(f_ptr == NULL)
        return -1;

    pthread_mutex_lock(&g_th_queue.qlock);
    if(g_th_queue.queue_len == TASK_QUEUE_SIZE)
    {
        // Queue is full
        pthread_mutex_unlock(&(g_th_queue.qlock));
        return -1;
    }
    else if(g_th_queue.queue_len == 0)
    {
        g_th_queue.first = 0;
        next = 0;
    }
    else
    {
        next = (g_th_queue.last + 1) % TASK_QUEUE_SIZE;
    }

    g_th_queue.queue[next].func = f_ptr;
    g_th_queue.queue[next].arg = arg;
    g_th_queue.last = next;
    g_th_queue.queue_len++;

    pthread_cond_signal(&(g_th_queue.task_avail));
    pthread_mutex_unlock(&(g_th_queue.qlock));
    return 0;
}

/**
 * Retireves a task struct from the queue,
 * the calling thread should have the qlock acquired
 * fills the task pointer with a function and argument
 */
static void get_task_from_queue(th_task *task)
{
    th_task *temp = NULL;
    task->func = NULL;
    task->arg = NULL;
    if(g_th_queue.queue_len == 0)
        return;
   
    temp = &g_th_queue.queue[g_th_queue.first];

    task->func = temp->func;
    task->arg = temp->arg;

    temp->func = NULL;
    temp->arg = NULL;
    g_th_queue.queue_len--;

    g_th_queue.first = (g_th_queue.first + 1) % TASK_QUEUE_SIZE;
}

/**
 * Main worker function to always keep running until
 * stop_threadpool is called, automatically fetches tasks
 * from queue and executes them.
 */
static void * thread_worker(void *arg)
{
    th_task task;
    while(g_th_queue.is_run)
    {
        pthread_mutex_lock(&g_th_queue.qlock);

        while (g_th_queue.queue_len == 0 && g_th_queue.is_run == true)
            pthread_cond_wait(&(g_th_queue.task_avail), &(g_th_queue.qlock));


        if (g_th_queue.is_run == false)
            break;

        get_task_from_queue(&task);
        pthread_mutex_unlock(&g_th_queue.qlock);

        if(task.func != NULL)
            task.func(task.arg);

    }
    pthread_mutex_unlock(&g_th_queue.qlock);
    return NULL;
}

int init_threadpool()
{
    int res = 0;
    size_t i = 0;
    pthread_attr_t thread_attr;

    g_th_queue.first = 0;
    g_th_queue.last  = 0;
    g_th_queue.is_run = true;
    g_th_queue.queue_len = 0;

    res = pthread_mutex_init(&(g_th_queue.qlock), NULL);
    if(res != 0)
        return -1;

    res = pthread_cond_init(&(g_th_queue.task_avail), NULL);
    if(res != 0)
        return -1;

    res = pthread_attr_init(&thread_attr);
    if(res != 0)
        return -1;

    res = pthread_attr_setstacksize(&thread_attr, (PTHREAD_STACK_MIN << 2));
    if ( res != 0)
        return -1;

    for(i = 0; i < TASK_QUEUE_SIZE; i++)
    {
        g_th_queue.queue[i].func = NULL;
        g_th_queue.queue[i].arg = NULL;
    }

    for(i = 0; i < THREAD_COUNT; i++)
    {
        res = pthread_create(&(g_th_queue.thread_arr[i]), &thread_attr, thread_worker, NULL);
        if(res != 0)
        {
            stop_threadpool();
            return -1;
        }
        pthread_detach(g_th_queue.thread_arr[i]);
    }
    return 0;
}

void stop_threadpool()
{
    size_t i = 0;

    pthread_mutex_lock(&(g_th_queue.qlock));
    g_th_queue.is_run = false;
    g_th_queue.queue_len = 0;
    for(i = 0; i < TASK_QUEUE_SIZE; i++)
    {
        g_th_queue.queue[i].func = NULL;
        g_th_queue.queue[i].arg = NULL;
    }
    pthread_cond_broadcast(&(g_th_queue.task_avail));
    pthread_mutex_unlock(&(g_th_queue.qlock));

    pthread_mutex_destroy(&(g_th_queue.qlock));
    pthread_cond_destroy(&(g_th_queue.task_avail));
}