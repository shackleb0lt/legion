#include "logger.h"

// FILE *log_file = NULL;
int log_fd = -1;
unsigned int line_count = 0;

static void rotate_logs()
{
    close(log_fd);
    rename(DEBUG_LOG_FILE, DEBUG_LOG_OLD);
    log_fd = open(DEBUG_LOG_FILE, O_WRONLY);
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
    close(log_fd);
#endif
}


/**
 * Debug function to write the logs to a file
 * Currently work in progress
 */
void logline(const char * prefix, const char *fmt, ...)
{
    va_list args;
    size_t buf_len = 0;
    time_t time_epoch = 0;
    struct tm time_local;
    char buffer[LOG_SIZE];

    time(&time_epoch);
    if (localtime_r(&time_epoch, &time_local) == NULL)
        return;

    buf_len = strftime(buffer, PREFIX_LEN, prefix, &time_local);

    va_start(args, fmt);
    buf_len += (size_t) vsnprintf(buffer + buf_len, LOG_SIZE - buf_len, fmt, args);
    va_end(args);

    write(log_fd, buffer, buf_len);

    line_count++;
    if(line_count == LOG_FILE_LIMIT)
        rotate_logs();
}