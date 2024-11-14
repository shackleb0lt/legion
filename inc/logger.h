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

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>

#include <error.h>
#include <errno.h>

#include <time.h>
#include <fcntl.h>
#include <unistd.h>
 
#define PREFIX_LEN      40
#define LOG_SIZE        1024
#define LOG_FILE_LIMIT  8192
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