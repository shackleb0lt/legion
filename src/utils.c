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

#include "server.h"
#include <sys/resource.h>

/**
 * Util fucntion to save ssl error to log file
 */
int ssl_log_err(const char *errstr, size_t len, void *u)
{
    (void)len;
    (void)u;
#ifndef DEBUG
    (void)errstr;
#endif
    LOG_ERROR("%s", errstr);
    return 0;
}

/**
 * Function that limits the total open file descriptor
 * for this process to MAX_FD_COUNT, we do not want
 * socket or file desscriptor numbers to exceed this num
 * because of the way we are storing the client connections
 */
int set_fd_limit()
{
    int ret = 0;
    struct rlimit rl;

    // Get the current limit
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Retrieval getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur == MAX_FD_COUNT && rl.rlim_max == MAX_FD_COUNT)
        return 0;

    // Set a new soft and hard limit for open files
    rl.rlim_cur = MAX_FD_COUNT;
    rl.rlim_max = MAX_FD_COUNT;

    // Set the new limit
    ret = setrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: setrlimit", __func__);
        return -1;
    }

    // Verify the change
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Verify getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur != MAX_FD_COUNT || rl.rlim_max != MAX_FD_COUNT)
    {
        LOG_ERROR("%s: Verification failed soft %ld, hard = %ld\n",
                  __func__, rl.rlim_cur, rl.rlim_max);
        return -1;
    }
    return 0;
}

/**
 * Convert a normal socket to a non-blocking socket
 * Returns 0 on success, -1 otherwise
 */
int set_nonblocking(const int fd)
{
    int ret = 0;
    ret = fcntl(fd, F_GETFL, 0);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl get", __func__);
        return -1;
    }

    ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl set", __func__);
        return -1;
    }
    return 0;
}