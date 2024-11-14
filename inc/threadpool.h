/**
 * MIT License
 *
 * Copyright (c) 2024 Aniruddha Kawade
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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