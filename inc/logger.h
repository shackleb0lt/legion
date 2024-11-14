#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
 
#define PREFIX_LEN 40
#define LOG_SIZE 1024
#define LOG_FILE_LIMIT 8192
#define DEBUG_LOG_FILE "/tmp/legion.log"
#define DEBUG_LOG_OLD  "/tmp/old_legion.log"

int init_logging();
void stop_logging();
void logline(const char *prefix, const char *fmt, ...);

#ifdef DEBUG
#define LOG_INFO(fmt, ...) logline("[INFO] [%Y-%m-%d %H:%M:%S] ", fmt "\n", ##__VA_ARGS__);
#define LOG_ERROR(fmt, ...) logline("[ERROR] [%Y-%m-%d %H:%M:%S] ", fmt "\n", ##__VA_ARGS__);
#else
#define LOG_INFO(fmt, ...)
#define LOG_ERROR(fmt, ...)
#endif


#endif