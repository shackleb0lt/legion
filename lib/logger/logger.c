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

#include "logger.h"

static int log_fd = -1;
static unsigned int line_count = 0;

static void rotate_logs()
{
    line_count = 0;
    close(log_fd);
    rename(DEBUG_LOG_FILE, DEBUG_LOG_OLD);
    log_fd = open(DEBUG_LOG_FILE, O_CREAT | O_WRONLY ,  0644);
    if(log_fd < 0)
    {
        perror("rotate_logs: open");
        exit(EXIT_FAILURE);
    }
}

/**
 * Check if previous log file existed,
 * if yes rename it to a backup file
 * and create new file for logging
 */
int init_logging()
{
#ifdef DEBUG
    if (access(DEBUG_LOG_FILE, F_OK) == 0)
    {
        rename(DEBUG_LOG_FILE, DEBUG_LOG_OLD);
    }

    log_fd = open(DEBUG_LOG_FILE, O_CREAT | O_WRONLY ,  0644);
    if (log_fd < 0)
    {
        perror("init_logging: open");
        return -1;
    }
#endif
    return 0;
}

/**
 * Clear write buffered log_file contents to memory
 * and close the file pointer
 */
void stop_logging()
{
#ifdef DEBUG
    if(log_fd < 0)
        return;

    close(log_fd);
    log_fd = -1;
#endif
}

/**
 * Debug function to write the logs to a file
 * Currently work in progress
 */
void logline(const char * prefix, const char *fmt, ...)
{
    va_list args;
    int errno_save = errno;
    size_t buf_len = 0;
    time_t time_epoch = 0;
    struct tm time_local;
    char buffer[LOG_SIZE];

    time(&time_epoch);
    if (localtime_r(&time_epoch, &time_local) == NULL)
        return;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    buf_len = strftime(buffer, PREFIX_LEN, prefix, &time_local);
#pragma GCC diagnostic pop

    if(prefix[1] == 'E' && errno != 0)
    {
        buf_len += (size_t) snprintf(buffer + buf_len, LOG_SIZE - buf_len - 1, "%s ", strerror(errno_save));
    }

    va_start(args, fmt);
    buf_len += (size_t) vsnprintf(buffer + buf_len, LOG_SIZE - buf_len - 1, fmt, args);
    va_end(args);

    buf_len = (size_t) write(log_fd, buffer, buf_len);

    line_count++;
    if(line_count == LOG_FILE_LIMIT)
        rotate_logs();
    errno = 0;
}